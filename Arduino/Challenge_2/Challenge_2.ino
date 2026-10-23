// Challenge_2.ino
// v4 - 2026-08-29
//
// A2 Challenge -- Voice commands for the A1 countdown timer.
//
// This supplied integration sketch must remain unchanged. Complete the model
// decision and prediction-debounce logic in command_policy.cpp.

#include "src/ECE116_A2_Keyword/ece116_audio.h"
#include "src/ECE116_A2_Keyword/ece116_keyword.h"

#include "a1_timer_logic.h"
#include "command_policy.h"

#define LED_STANDBY         2
#define LED_ADDING          3
#define LED_COUNTDOWN       4

#define ALERT_BLINK_MS      200UL
#define SERIAL_BAUD         115200

const char *const TIMER_MODE_NAMES[] = {"STANDBY", "ADDING", "COUNTDOWN", "DONE"};
const char *const COMMAND_EVENT_NAMES[] = {"NONE", "GO", "UP", "STOP"};

a1::TimerConfig makeA2TimerConfig();

// A2 preserves the A1 timer configuration except for its voice-command entry count.
const a1::TimerConfig timerConfiguration = makeA2TimerConfig();

ECE116Audio microphone;
int16_t *audioWindow = nullptr;

CommandPolicyConfig commandPolicyConfiguration;
CommandPolicyState commandPolicyState;
a1::TimerState timerState;

a1::TimerMode lastReportedTimerMode = (a1::TimerMode)255;
uint32_t lastReportedSeconds = UINT32_MAX;
uint32_t scoreWindowSequence = 0;

void setEveryLed(bool isOn);
void updateLedOutputs(uint32_t currentTimeMs);
void reportTimerStateChange(uint32_t currentTimeMs);
void reportScores(const ei_impulse_result_t &result,
                  float rootMeanSquare,
                  uint32_t capturedAtMs,
                  CommandEvent commandEvent);
a1::CoverEvent a1TimerEventForCommand(CommandEvent commandEvent);

/*
 * makeA2TimerConfig()
 *
 * Starts with the canonical A1 timer configuration and changes only the number
 * of UP events required to enter ADDING.
 */
a1::TimerConfig makeA2TimerConfig() {
  a1::TimerConfig config = a1::makeTimerConfig();
  config.entryTapsRequired = 1;
  return config;
}

/*
 * setup()
 *
 * Initializes the model input, A2 decision policy, and reused A1 timer state.
 */
void setup() {
  Serial.begin(SERIAL_BAUD);

  pinMode(LED_STANDBY,OUTPUT);
  pinMode(LED_ADDING,OUTPUT);
  pinMode(LED_COUNTDOWN,OUTPUT);

  audioWindow = static_cast<int16_t *>(ps_malloc(ECE116Audio::kSampleCount * sizeof(int16_t)));
  if (audioWindow == nullptr) {
    Serial.println("FAIL audio setup: PSRAM unavailable; select Tools > PSRAM > OPI PSRAM");
    while (true) yield();
  }
  if (!microphone.begin()) {
    Serial.print("FAIL audio setup: ");
    Serial.println(microphone.lastError());
    while (true) yield();
  }

  uint32_t setupTimeMs = millis();
  commandPolicyConfiguration = makeCommandPolicyConfig();
  resetCommandPolicy(&commandPolicyState,setupTimeMs);
  a1::resetTimer(&timerState,setupTimeMs);

  Serial.println("A2 Challenge: voice commands feed the A1 countdown timer");
  Serial.println("commands: up=add 10 seconds, go=start, stop=cancel");
  Serial.printf("policy accept=%.2f release=%.2f minimum_latch_ms=%u maximum_latch_ms=%u\n",
                commandPolicyConfiguration.acceptThreshold,
                commandPolicyConfiguration.releaseThreshold,
                commandPolicyConfiguration.minimumLatchMs,
                commandPolicyConfiguration.maximumLatchMs);
  reportTimerStateChange(setupTimeMs);
}

/*
 * loop()
 *
 * Runs the inherited A1 timer, then converts each available model score vector
 * into an accepted command event and an A1-compatible timer event.
 */
void loop() {
  uint32_t currentTimeMs = millis();
  a1::updateTimer(&timerState,a1::COVER_EVENT_NONE,currentTimeMs,timerConfiguration);
  updateLedOutputs(currentTimeMs);
  reportTimerStateChange(currentTimeMs);

  float rootMeanSquare = 0.0f;
  uint32_t capturedAtMs = 0;
  if (!microphone.copyLatestWindow(audioWindow,&rootMeanSquare,&capturedAtMs)) {
    yield();
    return;
  }

  signal_t signal;
  ei_impulse_result_t result = {};
  pcm16_signal_from_buffer(audioWindow,EI_CLASSIFIER_RAW_SAMPLE_COUNT,&signal);
  if (run_classifier(&signal,&result,false) != EI_IMPULSE_OK) return;

  CommandEvent commandEvent = updateCommandPolicy(&commandPolicyState,result.classification,capturedAtMs,commandPolicyConfiguration);
  if (commandEvent != COMMAND_EVENT_NONE) {
    a1::CoverEvent timerEvent = a1TimerEventForCommand(commandEvent);
    currentTimeMs = millis();
    a1::updateTimer(&timerState,timerEvent,currentTimeMs,timerConfiguration);
    Serial.print("event=");
    Serial.println(COMMAND_EVENT_NAMES[commandEvent]);
    reportTimerStateChange(currentTimeMs);
  }

  reportScores(result,rootMeanSquare,capturedAtMs,commandEvent);
}

/*
 * a1TimerEventForCommand()
 *
 * Adapts A2 voice-command events to the unchanged A1 timer event interface.
 */
a1::CoverEvent a1TimerEventForCommand(CommandEvent commandEvent) {
  switch (commandEvent) {
    case COMMAND_EVENT_UP:   return a1::COVER_EVENT_TAP;
    case COMMAND_EVENT_GO:   return a1::COVER_EVENT_COMMIT;
    case COMMAND_EVENT_STOP: return a1::COVER_EVENT_CANCEL;
    default:                 return a1::COVER_EVENT_NONE;
  }
}

/*
 * setEveryLed()
 *
 * Drives all three mode LEDs to the same level for shared visual patterns.
 */
void setEveryLed(bool isOn) {
  digitalWrite(LED_STANDBY,isOn);
  digitalWrite(LED_ADDING,isOn);
  digitalWrite(LED_COUNTDOWN,isOn);
}

/*
 * updateLedOutputs()
 *
 * Converts timer state into the same nonblocking LED feedback used in A1.
 */
void updateLedOutputs(uint32_t currentTimeMs) {
  if (timerState.mode == a1::TIMER_DONE) {
    setEveryLed(((currentTimeMs - timerState.doneStartMs) / ALERT_BLINK_MS) % 2 == 0);
    return;
  }

  digitalWrite(LED_STANDBY,timerState.mode == a1::TIMER_STANDBY);
  digitalWrite(LED_ADDING,timerState.mode == a1::TIMER_ADDING);
  digitalWrite(LED_COUNTDOWN,timerState.mode == a1::TIMER_COUNTDOWN);
}

/*
 * reportTimerStateChange()
 *
 * Reports timer mode or visible seconds only when either value changes.
 */
void reportTimerStateChange(uint32_t currentTimeMs) {
  bool timerModeDisplaysSeconds = timerState.mode == a1::TIMER_ADDING || timerState.mode == a1::TIMER_COUNTDOWN;
  uint32_t currentlyVisibleSeconds = timerModeDisplaysSeconds ? timerState.remainingSeconds : 0;
  if (lastReportedTimerMode == timerState.mode && lastReportedSeconds == currentlyVisibleSeconds) return;

  lastReportedTimerMode = timerState.mode;
  lastReportedSeconds = currentlyVisibleSeconds;

  if (timerModeDisplaysSeconds) {
    Serial.printf("status_ms=%u mode=%s seconds=%u\n",currentTimeMs,TIMER_MODE_NAMES[timerState.mode],currentlyVisibleSeconds);
  } else {
    Serial.printf("status_ms=%u mode=%s\n",currentTimeMs,TIMER_MODE_NAMES[timerState.mode]);
  }
}

/*
 * reportScores()
 *
 * Reports the complete model output and the event accepted from this window.
 */
void reportScores(const ei_impulse_result_t &result,
                  float rootMeanSquare,
                  uint32_t capturedAtMs,
                  CommandEvent commandEvent) {
  scoreWindowSequence += 1;
  Serial.print("SCORES window=");
  Serial.print(scoreWindowSequence);
  Serial.print(" captured_ms=");
  Serial.print(capturedAtMs);
  Serial.print(" rms=");
  Serial.print(rootMeanSquare,6);
  for (int classIndex = 0; classIndex < EI_CLASSIFIER_LABEL_COUNT; ++classIndex) {
    Serial.print(' ');
    Serial.print(result.classification[classIndex].label);
    Serial.print('=');
    Serial.print(result.classification[classIndex].value,4);
  }
  Serial.print(" event=");
  Serial.print(COMMAND_EVENT_NAMES[commandEvent]);
  Serial.print(" mode=");
  Serial.print(TIMER_MODE_NAMES[timerState.mode]);
  Serial.print(" dsp_ms=");
  Serial.print(result.timing.dsp);
  Serial.print(" cls_ms=");
  Serial.println(result.timing.classification);
}
