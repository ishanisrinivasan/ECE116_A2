#include "a1_timer_logic.h"

#include <limits.h>

namespace a1 {
namespace {

// Returns elapsed milliseconds using unsigned arithmetic so rollover is safe.
uint32_t elapsedMs(uint32_t nowMs, uint32_t sinceMs) {
  return (uint32_t)(nowMs - sinceMs);
}

const uint32_t MAX_COUNTDOWN_SECONDS = UINT32_MAX / 1000U;

// Restores every application-state field to its STANDBY invariant.
void clearTimerForStandby(TimerState *state, uint32_t nowMs) {
  state->mode = TIMER_STANDBY;
  state->entryTapCount = 0;
  state->lastEntryTapMs = nowMs;
  state->remainingSeconds = 0;
  state->countdownStartMs = nowMs;
  state->countdownDurationMs = 0;
  state->doneStartMs = nowMs;
}

}  // namespace

// Determines whether a periodic service is ready to run.
// Updates *lastRunMs only when the interval has elapsed and the service is due.
// Returns false for a null state pointer or when the service is not yet due.
bool periodicDue(uint32_t nowMs, uint32_t intervalMs, uint32_t *lastRunMs) {
  if (lastRunMs == nullptr) return false;
  if ((uint32_t)(nowMs - *lastRunMs) < intervalMs) return false;
  *lastRunMs = nowMs;
  return true;
}

// Returns the fixed entry count and selected application timing parameters.
TimerConfig makeTimerConfig() {
  return {800, 10, 2000, 3};
}

// Restores the application FSM to a complete STANDBY invariant.
void resetTimer(TimerState *state, uint32_t nowMs) {
  if (state == nullptr) return;
  clearTimerForStandby(state, nowMs);
}

// Applies one event and one elapsed-time update to the application FSM.
void updateTimer(TimerState *state,
                 CoverEvent event,
                 uint32_t nowMs,
                 const TimerConfig &config) {
  if (state == nullptr) return;

  switch (state->mode) {
    case TIMER_STANDBY: {
      if (state->entryTapCount > 0
          && elapsedMs(nowMs, state->lastEntryTapMs) > config.entryTapWindowMs) {
        state->entryTapCount = 0;
      }

      if (event == COVER_EVENT_TAP) {
        state->entryTapCount++;
        state->lastEntryTapMs = nowMs;
        uint8_t required = config.entryTapsRequired == 0
            ? 1 : config.entryTapsRequired;
        if (state->entryTapCount >= required) {
          state->mode = TIMER_ADDING;
          state->entryTapCount = 0;
          state->remainingSeconds = config.unitsPerTapSeconds > MAX_COUNTDOWN_SECONDS
              ? MAX_COUNTDOWN_SECONDS : config.unitsPerTapSeconds;
        }
      }
      break;
    }

    case TIMER_ADDING:
      if (event == COVER_EVENT_CANCEL) {
        clearTimerForStandby(state, nowMs);
      } else if (event == COVER_EVENT_TAP) {
        uint64_t total = (uint64_t)state->remainingSeconds
            + config.unitsPerTapSeconds;
        state->remainingSeconds = total > MAX_COUNTDOWN_SECONDS
            ? MAX_COUNTDOWN_SECONDS : (uint32_t)total;
      } else if (event == COVER_EVENT_COMMIT
                 && state->remainingSeconds > 0) {
        if (state->remainingSeconds > MAX_COUNTDOWN_SECONDS) {
          state->remainingSeconds = MAX_COUNTDOWN_SECONDS;
        }
        uint64_t duration = (uint64_t)state->remainingSeconds * 1000ULL;
        state->countdownDurationMs = (uint32_t)duration;
        state->countdownStartMs = nowMs;
        state->mode = TIMER_COUNTDOWN;
      }
      break;

    case TIMER_COUNTDOWN:
      if (event == COVER_EVENT_CANCEL) {
        clearTimerForStandby(state, nowMs);
      } else {
        uint32_t elapsed = elapsedMs(nowMs, state->countdownStartMs);
        if (elapsed >= state->countdownDurationMs) {
          state->remainingSeconds = 0;
          state->mode = TIMER_DONE;
          state->doneStartMs = nowMs;
        } else {
          uint32_t leftMs = state->countdownDurationMs - elapsed;
          state->remainingSeconds = (leftMs + 999U) / 1000U;
        }
      }
      break;

    case TIMER_DONE:
      if (elapsedMs(nowMs, state->doneStartMs) >= config.doneAlertMs) {
        clearTimerForStandby(state, nowMs);
      }
      break;

    default:
      clearTimerForStandby(state, nowMs);
      break;
  }
}

}  // namespace a1
