#ifndef ECE116_A2_KEYWORD_H
#define ECE116_A2_KEYWORD_H

#include <stddef.h>
#include <stdint.h>

#include "model_metadata.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*signal_get_data_t)(size_t offset, size_t length, float *out_ptr);

typedef struct {
    size_t total_length;
    signal_get_data_t get_data;
} signal_t;

typedef struct {
    const char *label;
    float value;
} ei_impulse_result_classification_t;

typedef struct {
    int sampling;
    int dsp;
    int classification;
    int anomaly;
} ei_impulse_result_timing_t;

typedef struct {
    ei_impulse_result_classification_t classification[EI_CLASSIFIER_LABEL_COUNT];
    float anomaly;
    ei_impulse_result_timing_t timing;
} ei_impulse_result_t;

typedef enum {
    EI_IMPULSE_OK = 0,
    EI_IMPULSE_ERROR_SHAPES_DONT_MATCH = -1,
    EI_IMPULSE_OUT_OF_MEMORY = -2,
    EI_IMPULSE_INFERENCE_ERROR = -7
} EI_IMPULSE_ERROR;

extern const char *ei_classifier_inferencing_categories[EI_CLASSIFIER_LABEL_COUNT];

int numpy_signal_from_buffer(float *data, size_t data_size, signal_t *signal);
int pcm16_signal_from_buffer(const int16_t *data, size_t data_size, signal_t *signal);
EI_IMPULSE_ERROR run_classifier(signal_t *signal, ei_impulse_result_t *result, bool debug);

#ifdef __cplusplus
}
#endif

#endif
