# A2 submission: Voice-controlled timer

Replace each prompt with your own evidence. Keep the headings.

## System summary

State what the completed system does. Identify the classifier output, the policy you implemented,
and the A1 timer behavior that remains unchanged.

## Model artifact

Record the repository URL, artifact ID, release tag, model/library version, sample rate, one-second
input size, and label order. State the roles of the metadata, parameter, interface, and
implementation files. Explain what `pcm16_signal_from_buffer()` and `run_classifier()` each do.

## Understanding one example

Include one complete `TRIAL_RESULT` from L4. Identify its true label, four-score output, argmax
label, and whether the argmax is correct. Explain why ground truth and model output must remain
separate.

## Provided benchmark

State what one row of `keyword_eval.csv` represents, the number of examples and speakers, and the
argmax accuracy. Embed `Fig/a2_confusion.png` with a caption that names the test unit and N. Identify
one measured confusion by true class, predicted class, and count. Explain its timer consequence and
include the relevant class metrics.

## Local model validation

Record the speaker, approximate distance, room condition, collection session, and non-command words
used for the 40 one-second trials. Report the class counts and embed
`Fig/a2_local_confusion.png`. Identify every valid trial whose argmax differs from its true label,
and separately note malformed or aborted attempts. Compare one local pattern with the provided
benchmark without extending the local result beyond these conditions.

## Operating policy

State the supplied 97% accepted-command precision requirement. Record the selected accept
threshold, release threshold, minimum latch time, maximum latch time, and exact rearm semantics.
Explain normal score release and why the maximum latch is a bounded fail-safe rather than evidence
that an utterance ended. Explain the timeout tradeoff between permanent lockout and possible later
repetition under a stuck high score. Explain the measured
precision/recall tradeoff and the consequence of a false accepted command. Embed
`Fig/a2_threshold_sweep.png`.

## Sequential replay

Report expected and observed events for every supplied trace. Use `class_shift_one_utterance` to
identify the accepted prediction, suppressed changes in the strongest class, and normal release
prediction. Use `maximum_latch_recovery` to identify the timeout window and the later accepted
event. Explain how your C++ implementation produces the same acceptance, suppression, normal
release, timeout recovery, and rearming decisions. For `student_normal_release` and
`student_maximum_latch`, include the complete rows, predicted events, observed events, and the
incorrect policy behavior each test was designed to expose. Include the complete output from
`check_command_policy.py` after implementing the C++ policy.

## Integrated system

Record board, core, PSRAM, LED, and command-to-A1 adapter settings. Report the required timer
sequence, five attempts per target command, and 10 non-command speech attempts. Include failed
attempts, classifying each as missed, wrong, repeated, or false when applicable. Add a
representative Serial excerpt with scores, event, mode, remaining seconds, and timing. Link
`Fig/voice_timer_demo.mp4`.

Use the `window` and `captured_ms` fields to associate each accepted or rejected score row with the
corresponding spoken attempt.

## Timing

Report the configured audio-window duration, configured capture stride, model-reported DSP time,
model-reported classifier time, and physically observed command-to-visible-response latency. Name
the source of each value. For physical latency, state that you measured from audible command onset
to the first matching LED or reported timer-state change, and describe the video timeline used.

## Limitations

Name at least two conditions not supported by your benchmark, local validation, or integrated
system evidence. Describe one next test that would reduce an important uncertainty.

## Build and reproduction

Give the exact steps and versions needed to obtain and place the fixed model release, run both
notebooks, compile both sketches, and reproduce the evidence. List every submitted file.
