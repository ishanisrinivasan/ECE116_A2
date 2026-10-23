#ifndef A1_TIMER_LOGIC_H
#define A1_TIMER_LOGIC_H

#include <stdint.h>

namespace a1 {

// These types and functions are copied from the canonical A1 timer interface.
// A2 supplies them unchanged because rebuilding the timer is not an A2 objective.

enum CoverEvent {
  COVER_EVENT_NONE = 0,  // No completed or held gesture became an event.
  COVER_EVENT_TAP,       // A completed brief cover fell in the tap interval.
  COVER_EVENT_COMMIT,    // A completed cover fell in the commit interval.
  COVER_EVENT_CANCEL     // A held cover reached the cancel threshold.
};

enum TimerMode {
  TIMER_STANDBY = 0,  // Wait for the required quick-tap entry sequence.
  TIMER_ADDING,       // Accumulate time units and wait for commit or cancel.
  TIMER_COUNTDOWN,    // Update remaining time from elapsed milliseconds.
  TIMER_DONE          // Display the completion alert before returning to standby.
};

struct TimerConfig {
  uint32_t entryTapWindowMs;   // Maximum allowed gap between entry taps, inclusive.
  uint32_t unitsPerTapSeconds; // Seconds seeded or added by one accepted tap.
  uint32_t doneAlertMs;        // Duration of the TIMER_DONE indication.
  uint8_t entryTapsRequired;   // Number of quick taps required to enter ADDING.
};

/*
 * makeTimerConfig()
 *
 * Returns the required entry count and selected timer timing parameters.
 */
TimerConfig makeTimerConfig();

struct TimerState {
  TimerMode mode;                 // Current observable application mode.
  uint8_t entryTapCount;          // Quick taps accepted in the current entry attempt.
  uint32_t lastEntryTapMs;        // millis() timestamp of the most recent entry tap.
  uint32_t remainingSeconds;      // Accumulated or rounded-up seconds, limited so milliseconds remain representable.
  uint32_t countdownStartMs;      // millis() timestamp when COUNTDOWN began.
  uint32_t countdownDurationMs;   // Fixed total countdown duration chosen at commit.
  uint32_t doneStartMs;           // millis() timestamp when TIMER_DONE began.
};

/*
 * periodicDue()
 *
 * Determines whether one periodic service is ready to run.
 *
 * Parameters:
 *   nowMs: Current time in milliseconds, normally the latest millis() value.
 *   intervalMs: Minimum milliseconds required between accepted service runs.
 *   lastRunMs: Mutable timestamp of the most recently accepted run. A null
 *     pointer is invalid and returns false.
 *
 * Returns:
 *   true when at least intervalMs has elapsed. A true result also updates
 *   *lastRunMs to nowMs. Otherwise returns false without changing *lastRunMs.
 *   Elapsed-time arithmetic remains correct across uint32_t rollover.
 */
bool periodicDue(uint32_t nowMs, uint32_t intervalMs, uint32_t *lastRunMs);

/*
 * resetTimer()
 *
 * Restores all application fields to the complete STANDBY invariant.
 *
 * Parameters:
 *   state: Timer state to initialize. A null pointer is ignored.
 *   nowMs: Current time in milliseconds; initializes all timer timestamps.
 */
void resetTimer(TimerState *state, uint32_t nowMs);

/*
 * updateTimer()
 *
 * Applies one gesture event and one elapsed-time update to the application FSM.
 *
 * Parameters:
 *   state: Persistent timer state from the previous update.
 *   event: Newly extracted cover event, or COVER_EVENT_NONE.
 *   nowMs: Current time in milliseconds.
 *   config: Entry-window, time-unit, DONE-duration, and required-tap settings.
 *
 * Effects:
 *   Updates the timer state with rollover-safe elapsed-time arithmetic. A null
 *   state pointer is ignored. An invalid mode recovers to STANDBY.
 */
void updateTimer(TimerState *state,
                 CoverEvent event,
                 uint32_t nowMs,
                 const TimerConfig &config);

}  // namespace a1

#endif
