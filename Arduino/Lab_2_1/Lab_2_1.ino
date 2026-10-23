// Lab_2_1.ino
// v6 - 2026-08-29
//
// Collects labeled, one-second audio trials with a released classifier.
// Complete TODO L3a and TODO L3b. The remaining capture and reporting code is supplied.

#include "src/ECE116_A2_Keyword/ece116_audio.h"
#include "src/ECE116_A2_Keyword/ece116_keyword.h"

#define SERIAL_BAUD 115200
#define TRIAL_BUTTON_PIN 5
#define RECORDING_LED_PIN 2

const uint32_t AUDIO_WINDOW_MS =
    1000UL * ECE116Audio::kSampleCount / ECE116Audio::kSampleRate;
const uint32_t AUDIO_CAPTURE_STRIDE_MS =
    1000UL * ECE116Audio::kBlockSamples / ECE116Audio::kSampleRate;
const uint32_t TRIAL_PREPARE_MS = 750;
const char *const TRIAL_LABELS[] = {"go", "up", "stop", "unknown"};
const int TRIAL_LABEL_COUNT = sizeof(TRIAL_LABELS) / sizeof(TRIAL_LABELS[0]);

ECE116Audio microphone;
int16_t *trialAudioWindow = nullptr;
uint32_t nextTrialId = 1;

void reportModelContract();
void reportNextTrial();
void collectOneSecondTrial();
int indexOfLargestScore(const ei_impulse_result_classification_t *scores);
void reportTrialResult(const ei_impulse_result_t &result,
                       const char *trueLabel,
                       float rootMeanSquare,
                       uint32_t recordingDurationMs,
                       uint32_t inferenceWallMs);

/*
 * setup()
 *
 * Starts the microphone and waits until one complete audio window exists before
 * inviting the student to collect the first trial.
 */
void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(TRIAL_BUTTON_PIN,INPUT_PULLUP);
  pinMode(RECORDING_LED_PIN,OUTPUT);
  digitalWrite(RECORDING_LED_PIN,LOW);

  trialAudioWindow = static_cast<int16_t *>(ps_malloc(ECE116Audio::kSampleCount * sizeof(int16_t)));
  if (trialAudioWindow == nullptr) {
    Serial.println("FAIL audio setup: PSRAM unavailable; select Tools > PSRAM > OPI PSRAM");
    while (true) yield();
  }
  if (!microphone.begin()) {
    Serial.print("FAIL audio setup: ");
    Serial.println(microphone.lastError());
    while (true) yield();
  }
  while (!microphone.copyLatestWindow(trialAudioWindow,nullptr,nullptr)) yield();

  reportModelContract();
  reportNextTrial();
}

/*
 * loop()
 *
 * Starts one trial for each accepted button press. Waiting for release prevents
 * one held press from starting a second trial.
 */
void loop() {
  if (digitalRead(TRIAL_BUTTON_PIN) != LOW) {
    yield();
    return;
  }

  delay(30);
  if (digitalRead(TRIAL_BUTTON_PIN) != LOW) return;

  collectOneSecondTrial();
  while (digitalRead(TRIAL_BUTTON_PIN) == LOW) yield();
  reportNextTrial();
}

/*
 * collectOneSecondTrial()
 *
 * Gives the student time to prepare, marks the recording interval with GPIO2,
 * and selects the first complete one-second model window recorded after the cue.
 */
void collectOneSecondTrial() {
  const char *trueLabel = TRIAL_LABELS[(nextTrialId - 1) % TRIAL_LABEL_COUNT];

  Serial.print("TRIAL_ARMED trial_id=");
  Serial.print(nextTrialId);
  Serial.print(" true_label=");
  Serial.println(trueLabel);
  delay(TRIAL_PREPARE_MS);

  uint32_t recordingStartedAtMs = 0;
  while (!microphone.copyLatestWindow(trialAudioWindow,nullptr,&recordingStartedAtMs)) yield();

  digitalWrite(RECORDING_LED_PIN,HIGH);
  Serial.print("RECORDING_START trial_id=");
  Serial.println(nextTrialId);

  float rootMeanSquare = 0.0f;
  uint32_t recordingEndedAtMs = recordingStartedAtMs;
  while (recordingEndedAtMs - recordingStartedAtMs < AUDIO_WINDOW_MS) {
    while (!microphone.copyLatestWindow(trialAudioWindow,&rootMeanSquare,&recordingEndedAtMs)) yield();
  }
  digitalWrite(RECORDING_LED_PIN,LOW);

  signal_t signal = {};
  ei_impulse_result_t result = {};
  int modelInputStatus = -1;  // TODO L3a: Replace -1 with pcm16_signal_from_buffer()
                              // using trialAudioWindow, the required sample count, and &signal.
  if (modelInputStatus != 0) {
    Serial.println("MODEL_INPUT_ERROR: connect the recorded PCM window to signal");
    return;
  }

  uint32_t inferenceStartedMs = millis();
  EI_IMPULSE_ERROR inferenceError = EI_IMPULSE_INFERENCE_ERROR;  // TODO L3b: Replace the
                                                                 // placeholder with
                                                                 // run_classifier(&signal,&result,false).
  uint32_t inferenceWallMs = millis() - inferenceStartedMs;
  if (inferenceError != EI_IMPULSE_OK) {
    Serial.print("INFERENCE_ERROR code=");
    Serial.println(static_cast<int>(inferenceError));
    return;
  }

  reportTrialResult(result,trueLabel,rootMeanSquare,recordingEndedAtMs - recordingStartedAtMs,inferenceWallMs);
  nextTrialId += 1;
}

/*
 * reportModelContract()
 *
 * Reports the dimensions and label order that define one model input and output.
 */
void reportModelContract() {
  Serial.print("MODEL_READY sample_rate_hz=");
  Serial.print(ECE116Audio::kSampleRate);
  Serial.print(" window_samples=");
  Serial.print(ECE116Audio::kSampleCount);
  Serial.print(" window_ms=");
  Serial.print(AUDIO_WINDOW_MS);
  Serial.print(" stride_ms=");
  Serial.print(AUDIO_CAPTURE_STRIDE_MS);
  Serial.print(" labels=");
  for (int classIndex = 0; classIndex < EI_CLASSIFIER_LABEL_COUNT; ++classIndex) {
    if (classIndex > 0) Serial.print(',');
    Serial.print(classIndex);
    Serial.print(':');
    Serial.print(ei_classifier_inferencing_categories[classIndex]);
  }
  Serial.println();
}

/*
 * reportNextTrial()
 *
 * Names the ground-truth label the student should produce for the next trial.
 */
void reportNextTrial() {
  Serial.print("NEXT_TRIAL trial_id=");
  Serial.print(nextTrialId);
  Serial.print(" expected_label=");
  Serial.print(TRIAL_LABELS[(nextTrialId - 1) % TRIAL_LABEL_COUNT]);
  Serial.println(" press GPIO5 when ready");
}

/*
 * indexOfLargestScore()
 *
 * Returns the first index containing the largest score in one model output.
 */
int indexOfLargestScore(const ei_impulse_result_classification_t *scores) {
  if (scores == nullptr || EI_CLASSIFIER_LABEL_COUNT <= 0) return -1;

  int largestScoreClassIndex = 0;
  for (int classIndex = 1; classIndex < EI_CLASSIFIER_LABEL_COUNT; ++classIndex) {
    if (scores[classIndex].value > scores[largestScoreClassIndex].value) {
      largestScoreClassIndex = classIndex;
    }
  }
  return largestScoreClassIndex;
}

/*
 * reportTrialResult()
 *
 * Reports one labeled example in a format that the Lab notebook can load.
 */
void reportTrialResult(const ei_impulse_result_t &result,
                       const char *trueLabel,
                       float rootMeanSquare,
                       uint32_t recordingDurationMs,
                       uint32_t inferenceWallMs) {
  int largestScoreClassIndex = indexOfLargestScore(result.classification);

  Serial.print("TRIAL_RESULT trial_id=");
  Serial.print(nextTrialId);
  Serial.print(" true_label=");
  Serial.print(trueLabel);
  Serial.print(" recording_ms=");
  Serial.print(recordingDurationMs);
  Serial.print(" rms=");
  Serial.print(rootMeanSquare,6);
  for (int classIndex = 0; classIndex < EI_CLASSIFIER_LABEL_COUNT; ++classIndex) {
    Serial.print(' ');
    Serial.print(result.classification[classIndex].label);
    Serial.print('=');
    Serial.print(result.classification[classIndex].value,4);
  }
  Serial.print(" argmax_index=");
  Serial.print(largestScoreClassIndex);
  Serial.print(" argmax_label=");
  Serial.print(result.classification[largestScoreClassIndex].label);
  Serial.print(" dsp_ms=");
  Serial.print(result.timing.dsp);
  Serial.print(" classifier_ms=");
  Serial.print(result.timing.classification);
  Serial.print(" inference_wall_ms=");
  Serial.println(inferenceWallMs);
}
