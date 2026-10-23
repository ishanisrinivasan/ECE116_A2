# ECE116 A2 Keyword Model Card

Model version: 1.0.0, built August 1, 2026.

Artifact ID: `ece116-keyword-commands`

Release tag: `keyword-commands-v1.0.0`

## Intended Use

This small model supports an instructional keyword-spotting system on the XIAO ESP32-S3 Sense.
It is designed to expose realistic score vectors, errors, threshold tradeoffs, and deployment
effects. It is not a safety system, accessibility product, identity system, or general speech
recognizer.

## Interface Contract

- Input: one second of mono signed 16-bit PCM at 16 kHz.
- On-device capture: overlapping windows updated every 250 ms.
- Preprocessing: DC centering for microphone PCM, pre-emphasis, 512-sample Hann frames,
  256-sample hop, 32 log-mel bands, and 16 pooled time bins.
- Classifier: 512 inputs, 64 ReLU hidden units, four-way softmax.
- Score order: `go`, `up`, `stop`, `unknown`.
- Output: four nonnegative scores that sum to approximately one.

Scores are relative evidence among these four classes. They are not calibrated probabilities
of user intent and are not commands.

## Data and Provenance

The source is TensorFlow's mini Speech Commands excerpt, downloaded from:

`https://storage.googleapis.com/download.tensorflow.org/data/mini_speech_commands.zip`

Archive SHA-256:

`49650f2341b26d886b46b3f4fb8fed59e30300b17550f1ee4a768b3106cf93a0`

Speech Commands was released under CC BY 4.0. Dataset and citation information:

- https://www.tensorflow.org/datasets/catalog/speech_commands
- https://arxiv.org/abs/1804.03209

Target classes use recorded `go`, `up`, and `stop` clips. Recorded `yes`, `no`, `down`,
`left`, and `right` clips form `unknown`.

## Split and Balance

Speaker identity is the filename prefix before `_nohash_`. A stable hash assigns each speaker
to exactly one split. No speaker appears in more than one split.

| Split | Clips | Speakers |
|---|---:|---:|
| Train | 2,823 | 1,058 |
| Validation | 555 | 200 |
| Test | 623 | 236 |

The unknown class is balanced across five source words and sampled to approximately the mean
target-class count in each split. The complete manifest and build script are retained in the
instructor model-source package.

## Held-Out Result

The exact released Python model obtains 83.63% argmax accuracy on the 623-clip speaker-disjoint
test set. This headline number is descriptive, not a recommended application policy. Students
must inspect the confusion matrix and select a consequence-based command threshold.

The released `keyword_eval.csv` contains the exact score vector, true label, source word,
speaker ID, clip ID, and RMS for every held-out example. The C++ path matched Python argmax on
12 fixed clips with maximum score difference below 0.000001 in the release check.

## Hardware Observation

On the connected XIAO ESP32-S3 Sense using Arduino-ESP32 3.3.10 with OPI PSRAM enabled:

- microphone capture initialized on GPIO41/GPIO42 at 16 kHz;
- observed model time was approximately 293 ms for DSP and 5 ms for classification;
- loudspeaker playback of held-out real recordings produced target scores above 0.97 for
  `go`, above 0.98 for `up`, and above 0.99 for `stop` in the recorded bench run.

These are observations from one board and one physical setup, not guaranteed specifications.

## Known Limitations

- The mini corpus contains short isolated words, not continuous conversational speech.
- The excerpt does not represent all accents, ages, speech patterns, rooms, microphones, or
  noise conditions.
- The unknown class contains five known words; it cannot represent every possible non-command.
- Silence and low-level board noise can still produce a high argmax, which is why rejection is
  required.
- A threshold improves one measured operating point but does not eliminate distribution shift.
- The float log-mel front end is intentionally transparent rather than maximally optimized.

## Responsible Interpretation

Report benchmark results and local-condition trials separately. Do not describe a few live
demonstrations as accuracy. Do not claim the scores measure intent. A missed command and a
false accepted command have different consequences, so the operating policy belongs to the
application rather than to the model file alone.
