#include "ece116_audio.h"

#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

namespace {
constexpr int kPdmClockPin = 42;
constexpr int kPdmDataPin = 41;

int16_t *allocateSamples(size_t count) {
    int16_t *memory = static_cast<int16_t *>(heap_caps_malloc(count * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!memory) memory = static_cast<int16_t *>(malloc(count * sizeof(int16_t)));
    return memory;
}
}

ECE116Audio::ECE116Audio()
    : task_(nullptr), ring_(nullptr), block_(nullptr), writePosition_(0), samplesFilled_(0),
      sequence_(0), captureMs_(0), rms_(0.0f), readyIndex_(0), lastReadSequence_(0),
      mux_(portMUX_INITIALIZER_UNLOCKED), error_("not started") {
    windows_[0] = nullptr;
    windows_[1] = nullptr;
}

void ECE116Audio::setError(const char *message) {
    error_ = message;
}

const char *ECE116Audio::lastError() const {
    return error_;
}

bool ECE116Audio::begin() {
    ring_ = allocateSamples(kSampleCount);
    block_ = allocateSamples(kBlockSamples);
    windows_[0] = allocateSamples(kSampleCount);
    windows_[1] = allocateSamples(kSampleCount);
    if (!ring_ || !block_ || !windows_[0] || !windows_[1]) {
        setError("audio buffer allocation failed");
        return false;
    }
    memset(ring_, 0, kSampleCount * sizeof(int16_t));
    i2s_.setPinsPdmRx(kPdmClockPin, kPdmDataPin);
    if (!i2s_.begin(I2S_MODE_PDM_RX, kSampleRate, I2S_DATA_BIT_WIDTH_16BIT,
                    I2S_SLOT_MODE_MONO, I2S_STD_SLOT_RIGHT)) {
        setError("PDM I2S initialization failed");
        return false;
    }
    BaseType_t created = xTaskCreatePinnedToCore(taskEntry, "ece116_audio", 4096, this, 2, &task_, 0);
    if (created != pdPASS) {
        setError("audio task creation failed");
        return false;
    }
    setError("ok");
    return true;
}

void ECE116Audio::taskEntry(void *context) {
    static_cast<ECE116Audio *>(context)->captureTask();
}

void ECE116Audio::captureTask() {
    while (true) {
        size_t expected = kBlockSamples * sizeof(int16_t);
        size_t received = i2s_.readBytes(reinterpret_cast<char *>(block_), expected);
        if (received != expected) continue;
        for (size_t index = 0; index < kBlockSamples; ++index) {
            ring_[writePosition_] = block_[index];
            writePosition_ = (writePosition_ + 1) % kSampleCount;
        }
        samplesFilled_ = min(samplesFilled_ + kBlockSamples, kSampleCount);
        if (samplesFilled_ < kSampleCount) continue;

        int next = 1 - readyIndex_;
        double sum = 0.0;
        double squareSum = 0.0;
        for (size_t index = 0; index < kSampleCount; ++index) {
            int16_t sample = ring_[(writePosition_ + index) % kSampleCount];
            windows_[next][index] = sample;
            float normalized = static_cast<float>(sample) / 32768.0f;
            sum += normalized;
            squareSum += normalized * normalized;
        }
        double mean = sum / kSampleCount;
        double variance = squareSum / kSampleCount - mean * mean;
        if (variance < 0.0) variance = 0.0;
        float level = sqrtf(static_cast<float>(variance));
        portENTER_CRITICAL(&mux_);
        readyIndex_ = next;
        rms_ = level;
        captureMs_ = millis();
        sequence_++;
        portEXIT_CRITICAL(&mux_);
    }
}

bool ECE116Audio::copyLatestWindow(int16_t *destination, float *level, uint32_t *capturedAtMs) {
    if (!destination) return false;
    uint32_t sequence;
    int index;
    float capturedLevel;
    uint32_t capturedTime;
    portENTER_CRITICAL(&mux_);
    sequence = sequence_;
    index = readyIndex_;
    capturedLevel = rms_;
    capturedTime = captureMs_;
    portEXIT_CRITICAL(&mux_);
    if (sequence == 0 || sequence == lastReadSequence_) return false;
    memcpy(destination, windows_[index], kSampleCount * sizeof(int16_t));
    lastReadSequence_ = sequence;
    if (level) *level = capturedLevel;
    if (capturedAtMs) *capturedAtMs = capturedTime;
    return true;
}
