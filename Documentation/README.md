# A2 submission: Voice-controlled timer

Replace each prompt with your own evidence. Keep the headings.

## System summary

State what the completed system does. Identify the classifier output, the policy you implemented,
and the A1 timer behavior that remains unchanged.

## Model artifact

| Field | Value |
|---|---|
| Repository | <https://github.com/ejaywang/ece116-tinyml-models> |
| Artifact ID | `ece116-keyword-commands` |
| Release tag | `keyword-commands-v1.0.0` (commit `e776497`) |
| Release asset | `ece116-keyword-arduino-v1.0.0.zip` |
| Model version | 1.0.0 (`artifact.json`, `MODEL_CARD.md`) |
| Library version | 1.0.0 (`library.properties`, library name `ECE116 A2 Keyword`) |
| Sample rate | 16 kHz (`EI_CLASSIFIER_FREQUENCY 16000`) |
| One-second input | 16,000 signed 16-bit mono PCM samples (`EI_CLASSIFIER_RAW_SAMPLE_COUNT 16000`) |
| Capture stride | 250 ms between the starts of consecutive windows (`capture_stride_ms` in `artifact.json`) |
| Label order | `go`, `up`, `stop`, `unknown` |
| Target | XIAO ESP32-S3 Sense, Espressif `esp32` core 3.3.10, OPI PSRAM |

The six source files were copied unchanged into `src/ECE116_A2_Keyword/` in both
`Arduino/Lab_2_1/` and `Arduino/Challenge_2/`. Every copy matches the SHA-256 hash recorded for it in
the release's `artifact.json`.

### File roles

| Role | File | Contents |
|---|---|---|
| Release record | `artifact.json` | Artifact ID, version, tag, target board, input/output contract, and a SHA-256 hash for every file. It is not compiled. |
| Metadata | `model_metadata.h` | The shape contract the sketch compiles against: 16 kHz sample rate, 16,000 raw samples per input, 512 network inputs, and 4 labels. It contains no learned values. |
| Parameters | `model_params.h` | The learned network weights and biases: `KW_W1` (512 × 64), `KW_B1` (64), `KW_W2` (64 × 4), and `KW_B2` (4). It also holds the fixed preprocessing constants: the Hann window, the 32-band mel filterbank, and the per-feature normalization mean and standard deviation. |
| Model interface | `ece116_keyword.h` | The types and calls the sketch uses: `signal_t`, `ei_impulse_result_t`, the error codes, `pcm16_signal_from_buffer()`, and `run_classifier()`. |
| Model implementation | `ece116_keyword.cpp` | The code behind that interface. It converts 16,000 samples into 512 features (pre-emphasis, 512-sample Hann frames with a 256-sample hop, FFT, 32 log-mel bands pooled into 16 time bins, normalization), then runs a 512 → 64 ReLU → 4 softmax network. |
| Audio interface | `ece116_audio.h` | The `ECE116Audio` class: `begin()`, `copyLatestWindow()`, and `lastError()`. |
| Audio implementation | `ece116_audio.cpp` | Starts the PDM microphone (clock GPIO42, data GPIO41) at 16 kHz and runs a background capture task. It keeps a one-second ring buffer in PSRAM and publishes a new window, with its RMS level and capture time, after each 4,000-sample (250 ms) block. |

Metadata tells the sketch how large the input and output are. Parameters decide what the model
predicts. Replacing `model_params.h` with differently trained weights would change the scores
without changing any interface the sketch depends on.

### Model calls

`pcm16_signal_from_buffer(trialAudioWindow, EI_CLASSIFIER_RAW_SAMPLE_COUNT, &signal)` prepares the
recorded audio as model input. It does not run the model or copy the samples. It records the buffer
pointer and length, computes the window's mean (DC offset), and installs a read callback in
`signal`. When the model reads through that callback, each 16-bit sample is returned as a float with
the mean removed and scaled by 1/32768 to the range [-1, 1). It returns 0 on success and -1 for a
null buffer or signal.

`run_classifier(&signal, &result, false)` performs inference. It rejects an input whose length is not
16,000 samples (`EI_IMPULSE_ERROR_SHAPES_DONT_MATCH`), reads the samples through the callback,
computes the 512 features, and evaluates the network. It writes four softmax scores into
`result.classification[]` in `go`, `up`, `stop`, `unknown` order, and the feature-extraction and
classifier times in milliseconds into `result.timing.dsp` and `result.timing.classification`. The
`debug` argument is ignored.

The output is a score vector, not a command. The four scores sum to about 1 and are relative
evidence among the four classes. The Challenge policy decides which score vectors become timer
events.

## Understanding one example

Trial 4 of the L4 trials:

```text
TRIAL_RESULT trial_id=4 true_label=unknown recording_ms=1006 rms=0.017803 go=0.7901 up=0.0164 stop=0.0986 unknown=0.0949 argmax_index=0 argmax_label=go dsp_ms=293 classifier_ms=5 inference_wall_ms=298
```

| Field | Value |
|---|---|
| True label | `unknown`: the sketch assigned this label before recording, and a non-command word was spoken |
| Score vector (`go`, `up`, `stop`, `unknown`) | 0.7901, 0.0164, 0.0986, 0.0949 (sum 1.0000) |
| Argmax label | `go` (index 0) |
| Argmax correct? | No. A non-command word was classified as `go`. |
| Input level | RMS 0.0178 over the one-second window |
| Processing | 293 ms feature extraction (`dsp_ms`) + 5 ms classifier = 298 ms wall time |

The true label records what the trial was set up to contain. The sketch fixes it before the
recording starts, and it does not depend on anything the model computes. The four scores and the
argmax are the model's measurement of that recording. They are relative evidence among four
classes, not a statement of what was said.

Keeping the two separate is what makes this error visible. If the model's output were used as the
label, this row would read as a correct `go` and would disappear from every error count. Evaluation
compares the two fields instead, so this trial adds one count to the (true `unknown`, predicted
`go`) cell of the confusion matrix. The comparison also shows the cost. If the argmax alone were
treated as a command, this non-command word would have started the countdown.

For contrast, trial 3 (true `stop`, `stop` = 0.9726) is a correct, confident prediction.

## Provided benchmark

One row of `keyword_eval.csv` is one held-out, labeled, one-second recording. It holds the clip ID,
the true label, the speaker ID, the recording's RMS level, and the released model's four scores
for that recording. The file has 623 rows with 623 unique clip IDs from 236 speakers. None of those
speakers was used for training or validation. Class counts are `go` 154, `up` 164, `stop` 149, and
`unknown` 156. Every score lies in [0, 1], and every row's four scores sum to 1 within 2 × 10⁻⁷.

Argmax accuracy is **0.8363** (521 of 623).

![Held-out confusion matrix](Fig/a2_confusion.png)
*Confusion matrix for the provided benchmark. Each count is one held-out one-second recording,
N = 623. Rows are true labels; columns are argmax predictions.*

| True \ Predicted | go | up | stop | unknown |
|---|---:|---:|---:|---:|
| **go** | 123 | 6 | 4 | 21 |
| **up** | 10 | 136 | 10 | 8 |
| **stop** | 1 | 7 | 139 | 2 |
| **unknown** | 19 | 11 | 3 | 123 |

| Class | Precision | Recall | Specificity | F1 |
|---|---:|---:|---:|---:|
| go | 0.804 | 0.799 | 0.936 | 0.801 |
| up | 0.850 | 0.829 | 0.948 | 0.840 |
| stop | 0.891 | 0.933 | 0.964 | 0.911 |
| unknown | 0.799 | 0.788 | 0.934 | 0.794 |

**Measured confusion:** 19 of the 156 true `unknown` recordings were predicted as `go`. Under argmax,
each of these would be a false accepted `go`: a non-command word would commit the accumulated time
and start the countdown. That is the main reason `go` precision is only 0.804, since 30 of the 153
`go` predictions are wrong and 19 of those 30 come from non-command speech. The opposite cell has a
different consequence. 21 true `go` recordings were predicted as `unknown`, and each of those is a
missed command that leaves the timer unchanged until the user repeats the word.

The most important application consequence is that false accepted commands change timer state
without intent, while missed commands only cost a repeat. The Challenge threshold is set against
the first kind. These numbers describe the held-out corpus recordings only.

## Local model validation

| Condition | Value |
|---|---|
| Speaker | One speaker (the author) |
| Microphone | XIAO ESP32-S3 Sense onboard PDM microphone, `Lab_2_1.ino` |
| Distance | About 25 cm from mouth to board |
| Room | Indoor room with some background noise |
| Session | One continuous session on October 23, 2026, trial IDs 1–40 after a board reset |
| Non-command words | Not written down during collection, so they cannot be listed per trial |

The complete log is `Documentation/a2_local_trials.txt`. It has 40 valid `TRIAL_RESULT` rows: 10
`go`, 10 `up`, 10 `stop`, and 10 `unknown`. Argmax accuracy on this set is 0.575 (23 of 40).

![Local confusion matrix](Fig/a2_local_confusion.png)
*Confusion matrix for the local validation set. Each count is one local one-second trial, N = 40.
Rows are true labels; columns are argmax predictions.*

| True \ Predicted | go | up | stop | unknown |
|---|---:|---:|---:|---:|
| **go** | 10 | 0 | 0 | 0 |
| **up** | 6 | 3 | 1 | 0 |
| **stop** | 1 | 0 | 9 | 0 |
| **unknown** | 6 | 1 | 2 | 1 |

**Valid trials whose argmax differs from the true label (17):**

| Trial | True | Argmax | Top score | RMS |
|---:|---|---|---:|---:|
| 2 | up | go | 0.6853 | 0.1281 |
| 4 | unknown | go | 0.9425 | 0.0098 |
| 6 | up | go | 0.5286 | 0.0011 |
| 8 | unknown | stop | 0.3770 | 0.0088 |
| 10 | up | go | 0.4103 | 0.0176 |
| 12 | unknown | go | 0.3903 | 0.0125 |
| 14 | up | go | 0.3736 | 0.0163 |
| 15 | stop | go | 0.4119 | 0.0107 |
| 16 | unknown | go | 0.4689 | 0.0057 |
| 20 | unknown | go | 0.9856 | 0.0247 |
| 24 | unknown | stop | 0.8681 | 0.0109 |
| 26 | up | stop | 0.3921 | 0.0113 |
| 30 | up | go | 0.3465 | 0.0143 |
| 32 | unknown | go | 0.9087 | 0.0098 |
| 34 | up | go | 0.4389 | 0.0170 |
| 36 | unknown | go | 0.7320 | 0.0108 |
| 40 | unknown | up | 0.4445 | 0.0065 |

**Malformed or aborted attempts:** none were observed. Every button press produced one complete
`TRIAL_RESULT`. The `TRIAL_ARMED` line for trial 8 is missing from the copied log, but its
`RECORDING_START` and `TRIAL_RESULT` lines are complete, so the row is valid. Trial 6 has a very low
RMS (0.0011), which suggests the word fell mostly outside the recording window. It is still a
complete trial and is kept.

**Comparison with the benchmark:** the clearest local pattern is over-prediction of `go`. All 10
local `go` trials were predicted correctly, but 13 of the 30 other trials were also predicted as
`go`: 6 of 10 `up`, 6 of 10 `unknown`, and 1 of 10 `stop`. On the benchmark the same cells are 10 of
164 `up` and 19 of 156 `unknown`. Local `stop` behaved much like the benchmark (9 of 10 locally,
139 of 149 on the benchmark).

These 40 trials cover one speaker, one room, one distance, and one session. They do not estimate
general model accuracy, and one trial moves a class rate by 10 percentage points. They do show
that, under these conditions, the model produces far more `go` predictions for non-`go` speech than
the held-out corpus suggests.

## Operating policy

**Requirement (supplied):** among benchmark windows accepted as `GO`, `UP`, or `STOP`, at least 97%
must have the correct target label.

| Value | Setting | Source |
|---|---|---|
| Accept threshold | 0.90 | Lowest tested threshold meeting the requirement (`Challenge_2_Policy.ipynb`) |
| Release threshold | 0.55 | Supplied |
| Minimum latch time | 1000 ms | Supplied; one full input-window duration |
| Maximum latch time | 1500 ms | Supplied fail-safe |

The same four values are in `makeCommandPolicyConfig()` in `Arduino/Challenge_2/command_policy.cpp`,
and `Challenge_2.ino` prints them at boot.

**Acceptance:** while armed, a window becomes a command event only when its argmax is `go`, `up`, or
`stop` and that score is at least 0.90. `unknown` never produces an event. The policy then latches
and records the event time.

**Exact rearm semantics:** while latched, every window returns `NONE`. With `elapsed = now −
latchTime` in unsigned 32-bit arithmetic, the policy rearms on a window when:

```text
normal release  = (go < 0.55 AND up < 0.55 AND stop < 0.55) AND elapsed >= 1000 ms
maximum timeout = elapsed >= 1500 ms
rearm           = normal release OR maximum timeout
```

The rearming window itself emits nothing. The earliest window that can produce a new event is the
next one.

**Normal release** is the evidence that the utterance has ended. All three target scores have
dropped, and at least one full one-second window has passed, so none of the audio that triggered
the event can still be in the model input. Scores alone could dip briefly between windows of the
same word. Time alone could rearm while the word is still in the window. Requiring both prevents
either kind of repeated event.

**The maximum latch is a bounded fail-safe.** A timeout says nothing about whether the utterance
ended. It only means 1500 ms passed while some target score stayed at or above 0.55, for example
because of a stuck score, a sustained noise, or a long word. Without it, one stuck high score would
lock out every later command permanently. The tradeoff is that after a timeout, a target score that
is still at or above 0.90 can be accepted again on the next window, so a stuck score can repeat an
event about every 1.75 s. A bounded repeat was chosen over permanent lockout because the user can
see and correct it, for example by saying `stop`.

**Precision/recall tradeoff:** at 0.90, the benchmark accepts 264 windows, 257 correctly, for a
command precision of **0.973** and a command recall of **0.550**. That leaves 7 false accepts. The
next lower tested threshold, 0.85, gives only 0.951 precision. A higher tested threshold, 0.95,
gives 0.986 precision but only 0.456 recall. Raising the threshold therefore rejects more real
commands and asks the user to repeat more often. Lowering it lets more false commands through. A
false accepted command adds 10 seconds, starts the countdown, or clears the timer without intent,
which is why the requirement constrains precision and the lowest qualifying threshold keeps as much
recall as possible.

![Threshold sweep](Fig/a2_threshold_sweep.png)
*Command precision and recall on the 623-example benchmark versus accept threshold. 0.90 is the
lowest tested threshold with precision ≥ 0.97.*

As a local check only, applying 0.90 to the 40 local trials accepts 19, of which 16 are correct
(precision 0.842, recall 0.533). The 3 false accepts are `unknown` trials 4, 20, and 32, each
accepted as `go`. Under these local conditions the benchmark operating point does not reach 97%
precision. This is one session from one speaker, but it is consistent with the local `go`
over-prediction above.

## Sequential replay

Replay of `score_traces.csv` at accept 0.90, release 0.55, minimum latch 1000 ms, and maximum
latch 1500 ms (`Challenge_2_Policy.ipynb`):

| Trace | Expected | Observed | Pass |
|---|---|---|---|
| `single_go` | 1 × go | go | ✅ |
| `single_up` | 1 × up | up | ✅ |
| `single_stop` | 1 × stop | stop | ✅ |
| `unknown_word` | 0 events | none | ✅ |
| `two_go` | 2 × go | go, go | ✅ |
| `class_shift_one_utterance` | 1 × go | go | ✅ |
| `maximum_latch_recovery` | 2 × go | go, go | ✅ |

**`class_shift_one_utterance`:** the accepted prediction is at 0 ms (`go` 0.95, armed → latched,
event `go`). The strongest class then changes three times while latched, and each window is
suppressed: `up` 0.74 at 250 ms, `stop` 0.92 at 500 ms, and `up` 0.90 at 750 ms. Without the latch,
the 500 ms and 750 ms windows would have been accepted as extra `stop` and `up` events. At 1000 ms
(`go` 0.10, `up` 0.12, `stop` 0.08, `unknown` 0.70) every target is below 0.55 and 1000 ms have
elapsed, so normal release rearms the policy. That window emits nothing.

**`maximum_latch_recovery`:** `go` stays at 0.95 in every window, so normal release never becomes
true. The windows from 250 ms through 1250 ms are suppressed. The timeout window is 1500 ms, where
`elapsed = 1500 ≥ 1500` rearms the policy with no event. The later accepted event is `go` at 1750 ms.
This is the bounded repetition the timeout trades for avoiding permanent lockout.

**C++ equivalence:** `updateCommandPolicy()` makes the same decisions in the same order as the
notebook's `trace_policy()`:

- **Acceptance:** while `commandIsLatched` is false, it accepts only an argmax index from 0 to 2
  whose score is at least `acceptThreshold`. It then sets `commandIsLatched = true` and
  `commandLatchedMs = nowMs`, and returns the mapped event.
- **Suppression:** while latched, the function always returns `COMMAND_EVENT_NONE`.
- **Normal release:** it loops over the three target scores to compute
  `everyTargetCommandScoreIsBelowReleaseThreshold`. Then `normalReleaseIsReady` is that value AND
  `commandHasBeenLatchedForMs >= minimumLatchMs`.
- **Timeout recovery:** `maximumLatchTimeHasElapsed` is `commandHasBeenLatchedForMs >=
  maximumLatchMs`.
- **Rearming:** if either condition holds, it clears `commandIsLatched`, and the window still
  returns `NONE`.
- **Rollover:** `commandHasBeenLatchedForMs = nowMs - commandLatchedMs` is unsigned 32-bit
  subtraction, so elapsed time stays correct when `millis()` wraps.

As an extra check, both student traces below were replayed through the compiled C++ policy, once
from time 0 and once starting 601 ms before `millis()` wraps past 2³². All four runs produced the
notebook's events (`up stop` and `go up`).

### `student_normal_release`

| time_ms | go | up | stop | unknown | Before | Event | After | Reason |
|---:|---:|---:|---:|---:|---|---|---|---|
| 0 | 0.02 | 0.93 | 0.02 | 0.03 | ARMED | up | LATCHED | accepted target |
| 250 | 0.01 | 0.96 | 0.01 | 0.02 | LATCHED | none | LATCHED | suppressed |
| 500 | 0.05 | 0.08 | 0.05 | 0.82 | LATCHED | none | LATCHED | quiet, but only 500 ms elapsed |
| 750 | 0.03 | 0.04 | 0.91 | 0.02 | LATCHED | none | LATCHED | class shift suppressed |
| 1000 | 0.03 | 0.05 | 0.62 | 0.30 | LATCHED | none | LATCHED | 1000 ms elapsed, but `stop` ≥ 0.55 |
| 1250 | 0.04 | 0.05 | 0.06 | 0.85 | LATCHED | none | ARMED | normal release |
| 1500 | 0.01 | 0.02 | 0.95 | 0.02 | ARMED | stop | LATCHED | accepted target |

Predicted `['up', 'stop']`; observed `['up', 'stop']`.

This test exposes a policy that rearms on quiet scores alone, without the minimum latch. That
policy would rearm at 500 ms, accept `stop` 0.91 at 750 ms, and produce `['up', 'stop', 'stop']`.
It also exposes a policy with no latch, which would fire again at 250 ms and 750 ms.

### `student_maximum_latch`

| time_ms | go | up | stop | unknown | Before | Event | After | Reason |
|---:|---:|---:|---:|---:|---|---|---|---|
| 0 | 0.96 | 0.01 | 0.01 | 0.02 | ARMED | go | LATCHED | accepted target |
| 250 | 0.95 | 0.01 | 0.01 | 0.03 | LATCHED | none | LATCHED | suppressed |
| 500 | 0.95 | 0.01 | 0.01 | 0.03 | LATCHED | none | LATCHED | suppressed |
| 750 | 0.94 | 0.02 | 0.01 | 0.03 | LATCHED | none | LATCHED | suppressed |
| 1000 | 0.94 | 0.02 | 0.01 | 0.03 | LATCHED | none | LATCHED | minimum met, `go` ≥ 0.55 |
| 1250 | 0.93 | 0.02 | 0.02 | 0.03 | LATCHED | none | LATCHED | suppressed |
| 1499 | 0.93 | 0.02 | 0.02 | 0.03 | LATCHED | none | LATCHED | 1 ms before maximum latch |
| 1500 | 0.93 | 0.02 | 0.02 | 0.03 | LATCHED | none | ARMED | maximum-latch recovery |
| 1750 | 0.02 | 0.95 | 0.01 | 0.02 | ARMED | up | LATCHED | accepted target |

Predicted `['go', 'up']`; observed `['go', 'up']`.

This test exposes a policy with no timeout, which stays latched under the stuck `go` and misses the
`up` at 1750 ms (`['go']`). It also exposes a policy whose timeout window emits an event, which
would fire `go` again at 1500 ms (`['go', 'go', 'up']`). A policy that rearms on minimum time alone
would rearm at 1000 ms and accept `go` again at 1250 ms.

### `check_command_policy.py` output

```text
PASS policy values
PASS class-to-event mapping
PASS sub-threshold rejection
PASS accepted command
PASS class-shift suppression
PASS minimum latch time
PASS quiet release window
PASS later utterance after release
PASS maximum latch not early
PASS maximum latch timeout window
PASS later utterance after timeout
Public command-policy checks: 11/11
```

The two rearm checks map to rows in the student traces. "quiet release window" is the normal
release at 1250 ms in `student_normal_release`, and "maximum latch timeout window" is the 1500 ms
timeout in `student_maximum_latch`.

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

### Model release

1. Download `ece116-keyword-arduino-v1.0.0.zip` from the fixed `keyword-commands-v1.0.0` release:
   <https://github.com/ejaywang/ece116-tinyml-models/releases/download/keyword-commands-v1.0.0/ece116-keyword-arduino-v1.0.0.zip>.
   Do not substitute a later release.
2. Extract the ZIP outside the A2 package.
3. Copy the six files from the extracted `arduino/ECE116_A2_Keyword/src/` folder, unchanged, into
   both sketch-local folders:
   - `Arduino/Lab_2_1/src/ECE116_A2_Keyword/`
   - `Arduino/Challenge_2/src/ECE116_A2_Keyword/`

   The files are `ece116_audio.cpp`, `ece116_audio.h`, `ece116_keyword.cpp`, `ece116_keyword.h`,
   `model_metadata.h`, and `model_params.h`. The model is not installed as a global Arduino
   library, so each sketch carries its own copy of the dependency.
4. Confirm each copy against the SHA-256 hash recorded for it in the release's `artifact.json`.
   All 12 copies match.

### Arduino build

| Setting | Value |
|---|---|
| Board package | `esp32` by Espressif Systems, version 3.3.10 |
| Board | `XIAO_ESP32S3` (Seeed Studio XIAO ESP32-S3 Sense) |
| PSRAM | Tools > PSRAM > OPI PSRAM |
| Serial Monitor | 115200 baud |
| Upload port | The board's USB modem port (`/dev/cu.usbmodem1101` on the Mac used here) |

Open the sketch folder in Arduino IDE, apply the settings above, and click Verify. `Lab_2_1.ino`
compiled with the model source in place: "Sketch uses 322457 bytes (9%) of program storage space"
and "Global variables use 22340 bytes (6%) of dynamic memory."

If the board port is missing or an upload cannot connect, close Serial Monitor, hold `BOOT`, tap
and release `RESET`, release `BOOT`, select the new USB modem port, and upload again. Tap `RESET`
if the uploaded sketch does not start.

### Notebooks and policy self-check

Both notebooks were executed as local Jupyter, top to bottom, from the package's `Python/` directory
(Python 3.13.17, NumPy 2.5.3, Matplotlib 3.11.2) on Linux. Colab is the supported alternative:
upload the notebook, then the complete package ZIP when the setup cell asks.

1. `Lab_2_2_Evaluation.ipynb` reads `keyword_eval.csv` and `../Documentation/a2_local_trials.txt`.
   It writes `Fig/a2_confusion.png` and `Fig/a2_local_confusion.png`.
2. `Challenge_2_Policy.ipynb` reads `keyword_eval.csv`, `score_traces.csv`, and
   `trace_manifest.csv`. It writes `Fig/a2_threshold_sweep.png`.
3. From `Python/`, `python check_command_policy.py` compiles `command_policy.cpp` with the host
   compiler (g++ 13.3.0 here, on Linux; not the Arduino ESP32 compiler) and prints the named checks.

### Still to record

Add the `Challenge_2.ino` compile result, the integrated-system steps, and the list of every
submitted file.
