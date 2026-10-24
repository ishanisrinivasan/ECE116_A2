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

### Still to record

Add the notebook environment (Colab or local Jupyter), the policy self-check environment, the
`Challenge_2.ino` compile result, the steps to reproduce each piece of evidence, and the list of
every submitted file.
