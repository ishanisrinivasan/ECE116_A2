#include "ece116_keyword.h"
#include "model_params.h"

#include <Arduino.h>
#include <math.h>

const char *ei_classifier_inferencing_categories[EI_CLASSIFIER_LABEL_COUNT] = {
    "go", "up", "stop", "unknown"
};

static float *g_float_buffer = nullptr;
static const int16_t *g_pcm_buffer = nullptr;
static size_t g_buffer_length = 0;
static float g_pcm_mean = 0.0f;

extern "C" int ece116_get_float_data(size_t offset, size_t length, float *output) {
    for (size_t index = 0; index < length; ++index) {
        output[index] = (offset + index < g_buffer_length) ? g_float_buffer[offset + index] : 0.0f;
    }
    return 0;
}

extern "C" int ece116_get_pcm_data(size_t offset, size_t length, float *output) {
    for (size_t index = 0; index < length; ++index) {
        output[index] = (offset + index < g_buffer_length)
            ? (static_cast<float>(g_pcm_buffer[offset + index]) - g_pcm_mean) / 32768.0f : 0.0f;
    }
    return 0;
}

int numpy_signal_from_buffer(float *data, size_t data_size, signal_t *signal) {
    if (!data || !signal) return -1;
    g_float_buffer = data;
    g_pcm_buffer = nullptr;
    g_buffer_length = data_size;
    signal->total_length = data_size;
    signal->get_data = &ece116_get_float_data;
    return 0;
}

int pcm16_signal_from_buffer(const int16_t *data, size_t data_size, signal_t *signal) {
    if (!data || !signal) return -1;
    g_pcm_buffer = data;
    g_float_buffer = nullptr;
    g_buffer_length = data_size;
    double sum = 0.0;
    for (size_t index = 0; index < data_size; ++index) sum += data[index];
    g_pcm_mean = data_size > 0 ? static_cast<float>(sum / data_size) : 0.0f;
    signal->total_length = data_size;
    signal->get_data = &ece116_get_pcm_data;
    return 0;
}

static void fft_radix2(float *real, float *imaginary, int count) {
    for (int index = 1, reverse = 0; index < count; ++index) {
        int bit = count >> 1;
        for (; reverse & bit; bit >>= 1) reverse ^= bit;
        reverse ^= bit;
        if (index < reverse) {
            float temp = real[index]; real[index] = real[reverse]; real[reverse] = temp;
            temp = imaginary[index]; imaginary[index] = imaginary[reverse]; imaginary[reverse] = temp;
        }
    }
    for (int length = 2; length <= count; length <<= 1) {
        int half = length >> 1;
        for (int base = 0; base < count; base += length) {
            for (int offset = 0; offset < half; ++offset) {
                float angle = -2.0f * static_cast<float>(M_PI) * offset / length;
                float wr = cosf(angle);
                float wi = sinf(angle);
                float xr = real[base + offset + half];
                float xi = imaginary[base + offset + half];
                float vr = xr * wr - xi * wi;
                float vi = xr * wi + xi * wr;
                float ur = real[base + offset];
                float ui = imaginary[base + offset];
                real[base + offset] = ur + vr;
                imaginary[base + offset] = ui + vi;
                real[base + offset + half] = ur - vr;
                imaginary[base + offset + half] = ui - vi;
            }
        }
    }
}

static float audio_buffer[KW_NSAMP];
static float fft_real[KW_NFFT];
static float fft_imaginary[KW_NFFT];
static float feature_buffer[KW_NFEAT];

static void compute_features() {
    float pooled[KW_NTIME * KW_NMEL];
    int counts[KW_NTIME];
    for (int index = 0; index < KW_NTIME * KW_NMEL; ++index) pooled[index] = 0.0f;
    for (int index = 0; index < KW_NTIME; ++index) counts[index] = 0;

    for (int frame = 0; frame < KW_NFRAME; ++frame) {
        int base = frame * KW_HOP;
        for (int index = 0; index < KW_NFFT; ++index) {
            int audio_index = base + index;
            float current = audio_buffer[audio_index];
            float previous = audio_index > 0 ? audio_buffer[audio_index - 1] : 0.0f;
            fft_real[index] = (current - KW_PREEMPH * previous) * KW_HANN[index];
            fft_imaginary[index] = 0.0f;
        }
        fft_radix2(fft_real, fft_imaginary, KW_NFFT);
        int time_index = min((frame * KW_NTIME) / KW_NFRAME, KW_NTIME - 1);
        for (int mel = 0; mel < KW_NMEL; ++mel) {
            float energy = 0.0f;
            const float *filter = &KW_MELFB[mel * KW_NBIN];
            for (int bin = 0; bin < KW_NBIN; ++bin) {
                float power = fft_real[bin] * fft_real[bin] + fft_imaginary[bin] * fft_imaginary[bin];
                energy += filter[bin] * power;
            }
            pooled[time_index * KW_NMEL + mel] += logf(energy + KW_EPS);
        }
        counts[time_index]++;
    }
    for (int time_index = 0; time_index < KW_NTIME; ++time_index) {
        float divisor = counts[time_index] > 0 ? static_cast<float>(counts[time_index]) : 1.0f;
        for (int mel = 0; mel < KW_NMEL; ++mel) {
            int index = time_index * KW_NMEL + mel;
            float value = pooled[index] / divisor;
            feature_buffer[index] = (value - KW_MEAN[index]) / KW_STD[index];
        }
    }
}

EI_IMPULSE_ERROR run_classifier(signal_t *signal, ei_impulse_result_t *result, bool debug) {
    (void)debug;
    if (!signal || !result || !signal->get_data) return EI_IMPULSE_INFERENCE_ERROR;
    if (signal->total_length != KW_NSAMP) return EI_IMPULSE_ERROR_SHAPES_DONT_MATCH;

    uint32_t dsp_start = micros();
    signal->get_data(0, KW_NSAMP, audio_buffer);
    compute_features();
    uint32_t dsp_end = micros();

    uint32_t classifier_start = micros();
    float hidden[KW_HIDDEN];
    for (int hidden_index = 0; hidden_index < KW_HIDDEN; ++hidden_index) {
        float accumulator = KW_B1[hidden_index];
        const float *weights = &KW_W1[hidden_index * KW_NFEAT];
        for (int feature = 0; feature < KW_NFEAT; ++feature) {
            accumulator += weights[feature] * feature_buffer[feature];
        }
        hidden[hidden_index] = max(accumulator, 0.0f);
    }

    float logits[KW_NCLASS];
    float maximum = -1e30f;
    for (int class_index = 0; class_index < KW_NCLASS; ++class_index) {
        float accumulator = KW_B2[class_index];
        const float *weights = &KW_W2[class_index * KW_HIDDEN];
        for (int hidden_index = 0; hidden_index < KW_HIDDEN; ++hidden_index) {
            accumulator += weights[hidden_index] * hidden[hidden_index];
        }
        logits[class_index] = accumulator;
        maximum = max(maximum, accumulator);
    }
    float total = 0.0f;
    for (int class_index = 0; class_index < KW_NCLASS; ++class_index) {
        logits[class_index] = expf(logits[class_index] - maximum);
        total += logits[class_index];
    }
    for (int class_index = 0; class_index < KW_NCLASS; ++class_index) {
        result->classification[class_index].label = ei_classifier_inferencing_categories[class_index];
        result->classification[class_index].value = logits[class_index] / total;
    }
    uint32_t classifier_end = micros();

    result->anomaly = 0.0f;
    result->timing.sampling = 0;
    result->timing.dsp = static_cast<int>((dsp_end - dsp_start + 500) / 1000);
    result->timing.classification = static_cast<int>((classifier_end - classifier_start + 500) / 1000);
    result->timing.anomaly = 0;
    return EI_IMPULSE_OK;
}
