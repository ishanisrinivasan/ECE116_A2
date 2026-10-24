#ifndef ECE116_A2_AUDIO_H
#define ECE116_A2_AUDIO_H

#include <Arduino.h>
#include <ESP_I2S.h>

class ECE116Audio {
public:
    static constexpr size_t kSampleCount = 16000;
    static constexpr size_t kBlockSamples = 4000;
    static constexpr uint32_t kSampleRate = 16000;

    ECE116Audio();
    bool begin();
    bool copyLatestWindow(int16_t *destination, float *rms, uint32_t *capturedAtMs);
    const char *lastError() const;

private:
    static void taskEntry(void *context);
    void captureTask();
    void setError(const char *message);

    I2SClass i2s_;
    TaskHandle_t task_;
    int16_t *ring_;
    int16_t *block_;
    int16_t *windows_[2];
    size_t writePosition_;
    size_t samplesFilled_;
    volatile uint32_t sequence_;
    volatile uint32_t captureMs_;
    volatile float rms_;
    volatile int readyIndex_;
    uint32_t lastReadSequence_;
    portMUX_TYPE mux_;
    const char *error_;
};

#endif
