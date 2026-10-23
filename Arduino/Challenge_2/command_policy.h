#ifndef ECE116_A2_COMMAND_POLICY_H
#define ECE116_A2_COMMAND_POLICY_H

#include <stdint.h>

#include "src/ECE116_A2_Keyword/ece116_keyword.h"

enum CommandEvent {
  COMMAND_EVENT_NONE = 0,  // No accepted command event occurred on this window.
  COMMAND_EVENT_GO,        // Commit the accumulated time and start the countdown.
  COMMAND_EVENT_UP,        // Enter time-setting mode or add one 10-second unit.
  COMMAND_EVENT_STOP       // Cancel the timer and return it to STANDBY.
};

struct CommandPolicyConfig {
  float acceptThreshold;      // Minimum top target-class score accepted as a command.
  float releaseThreshold;     // Every target score must be below this value before rearming.
  uint32_t minimumLatchMs;    // Minimum time after an event before rearming is allowed.
  uint32_t maximumLatchMs;    // Fail-safe time after which a stuck latch rearms.
};

struct CommandPolicyState {
  bool commandIsLatched;      // Whether one accepted utterance is suppressing nearby windows.
  uint32_t commandLatchedMs;  // millis() timestamp when the current command event fired.
};

/*
 * makeCommandPolicyConfig()
 *
 * Returns the operating point evaluated in the Challenge notebook.
 */
CommandPolicyConfig makeCommandPolicyConfig();

/*
 * resetCommandPolicy()
 *
 * Clears accepted-command history without emitting an event.
 *
 * Parameters:
 *   state: Policy state to initialize. A null pointer is ignored.
 *   nowMs: Current time in milliseconds; initializes the latch timestamp.
 */
void resetCommandPolicy(CommandPolicyState *state, uint32_t nowMs);

/*
 * indexOfLargestScore()
 *
 * Finds the class with the largest value in one model score vector.
 *
 * Parameters:
 *   scores: Array of EI_CLASSIFIER_LABEL_COUNT classification records in the
 *     label order defined by the released model.
 *
 * Returns:
 *   The first class index with the largest score. Returns -1 for a null array
 *   or when the released model has no labels.
 */
int indexOfLargestScore(const ei_impulse_result_classification_t *scores);

/*
 * commandEventForClassIndex()
 *
 * Maps the released model's target-class indices to application events.
 *
 * Parameters:
 *   classIndex: Model class index. The released order is go, up, stop, unknown.
 *
 * Returns:
 *   The matching command event for indices 0 through 2. Unknown and invalid
 *   indices return COMMAND_EVENT_NONE.
 */
CommandEvent commandEventForClassIndex(int classIndex);

/*
 * updateCommandPolicy()
 *
 * Converts one model score vector into at most one debounced command event.
 *
 * Parameters:
 *   state: Persistent policy state from the previous model window.
 *   scores: Current score vector in the released model's label order.
 *   nowMs: Timestamp associated with the current prediction window.
 *   config: Accept, release, minimum-latch, and maximum-latch policy values.
 *
 * Returns:
 *   One command event when an unlatched target class meets the accept
 *   threshold. Unknown classes, rejected windows, adjacent windows from an
 *   accepted utterance, invalid inputs, and the release window return
 *   COMMAND_EVENT_NONE. A latched command normally rearms after minimumLatchMs
 *   has elapsed and every target-command score is below releaseThreshold. It
 *   also rearms after maximumLatchMs as a bounded fail-safe. A rearming window
 *   never emits an event.
 */
CommandEvent updateCommandPolicy(CommandPolicyState *state,
                                 const ei_impulse_result_classification_t *scores,
                                 uint32_t nowMs,
                                 const CommandPolicyConfig &config);

#endif
