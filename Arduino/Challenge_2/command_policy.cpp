#include "command_policy.h"

/*
 * makeCommandPolicyConfig()
 *
 * Returns the operating point evaluated in the Challenge notebook.
 */
CommandPolicyConfig makeCommandPolicyConfig() {
  CommandPolicyConfig config{};  // Creates the policy returned to Challenge_2.ino.

  config.acceptThreshold = 1.01f;    // TODO C3a: replace 1.01f with ACCEPT_THRESHOLD from your notebook.
  config.releaseThreshold = 0.55f;   // Requires target scores to fall below this value before rearming.
  config.minimumLatchMs = 1000;      // Suppresses predictions for at least one complete input-window duration.
  config.maximumLatchMs = 1500;      // Bounds lockout if target scores never reach normal release.

  return config;  // Gives one consistent policy to the integration sketch.
}

/*
 * resetCommandPolicy()
 *
 * Clears accepted-command history without emitting an event.
 *
 * Parameters:
 *   state: Policy state to initialize. A null pointer is ignored.
 *   nowMs: Current time in milliseconds; initializes the latch timestamp.
 */
void resetCommandPolicy(CommandPolicyState *state, uint32_t nowMs) {
  if (state == nullptr) return;  // Cannot initialize missing state.

  state->commandIsLatched = false;  // Starts ready to accept a command.
  state->commandLatchedMs = nowMs;  // Gives future elapsed-time checks a known origin.
}

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
int indexOfLargestScore(const ei_impulse_result_classification_t *scores) {
  if (scores == nullptr || EI_CLASSIFIER_LABEL_COUNT <= 0) return -1;  // Rejects an invalid score vector.

  int largestScoreClassIndex = 0;                                                   // Uses the first class as the initial candidate.
  for (int classIndex = 1; classIndex < EI_CLASSIFIER_LABEL_COUNT; ++classIndex) {  // Checks every remaining class.
    if (scores[classIndex].value > scores[largestScoreClassIndex].value) {           // Compares this score with the current largest score.
      largestScoreClassIndex = classIndex;                                          // Retains the first class when scores are tied.
    }
  }

  return largestScoreClassIndex;  // Returns the argmax class index.
}

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
CommandEvent commandEventForClassIndex(int classIndex) {
  switch (classIndex) {           // Selects the event associated with the model class.
    case 0:                       // Handles the released GO class index.
      return COMMAND_EVENT_GO;    // Maps the model class to the application event.
    case 1:                       // Handles the released UP class index.
      return COMMAND_EVENT_UP;    // Maps the model class to the application event.
    case 2:                       // Handles the released STOP class index.
      return COMMAND_EVENT_STOP;  // Maps the model class to the application event.
    default:                      // Handles UNKNOWN and invalid class indices.
      return COMMAND_EVENT_NONE;  // Prevents non-target classes from becoming commands.
  }
}

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
                                 const CommandPolicyConfig &config) {
  if (state == nullptr || scores == nullptr) return COMMAND_EVENT_NONE;  // Rejects invalid inputs.

  if (state->commandIsLatched) {                                         // Handles windows adjacent to an accepted utterance.
    bool everyTargetCommandScoreIsBelowReleaseThreshold = true;          // Tracks whether GO, UP, and STOP have all released.
    for (int targetClassIndex = 0; targetClassIndex < 3; ++targetClassIndex) {  // Checks every target command class.
      if (scores[targetClassIndex].value >= config.releaseThreshold) {    // A score at the boundary has not released.
        everyTargetCommandScoreIsBelowReleaseThreshold = false;          // Keeps the latch active while any target score remains high.
      }
    }
    uint32_t commandHasBeenLatchedForMs = nowMs - state->commandLatchedMs;  // Uses rollover-safe unsigned subtraction.
    bool minimumLatchTimeHasElapsed = false;                                // TODO C3b: compare elapsed time with minimumLatchMs.
    bool maximumLatchTimeHasElapsed = false;                                // TODO C3c: compare elapsed time with maximumLatchMs.
    bool normalReleaseIsReady = false;                                      // TODO C3d: combine quiet target scores with minimum latch time.

    if (normalReleaseIsReady || maximumLatchTimeHasElapsed) {  // Uses score release normally and timeout only as a fail-safe.
      state->commandIsLatched = false;                       // Allows a later window to become a new event.
    }

    return COMMAND_EVENT_NONE;  // A normal-release or timeout rearming window never emits an event.
  }

  int largestScoreClassIndex = indexOfLargestScore(scores);  // Finds the current argmax class.
  bool largestScoreClassIsTargetCommand = largestScoreClassIndex >= 0
      && largestScoreClassIndex <= 2;  // Accepts only GO, UP, and STOP class indices.
  bool largestScoreMeetsAcceptThreshold = largestScoreClassIndex >= 0
      && scores[largestScoreClassIndex].value >= config.acceptThreshold;

  if (!largestScoreClassIsTargetCommand || !largestScoreMeetsAcceptThreshold) {  // Rejects unknown or uncertain windows.
    return COMMAND_EVENT_NONE;                                                   // Keeps rejected model output out of the timer.
  }

  CommandEvent acceptedCommandEvent = commandEventForClassIndex(largestScoreClassIndex);  // Converts the accepted class into an event.
  if (acceptedCommandEvent == COMMAND_EVENT_NONE) return COMMAND_EVENT_NONE;              // Rejects an invalid class-to-event mapping.

  state->commandIsLatched = false;  // TODO C3e: mark the accepted utterance as latched.
  state->commandLatchedMs = 0;      // TODO C3f: record when the accepted command fired.

  return acceptedCommandEvent;  // Emits exactly one event for this accepted window.
}
