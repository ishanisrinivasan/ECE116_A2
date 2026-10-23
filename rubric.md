# A2 Grading Rubric

A2 is graded out of 100 points and contributes 18.75 percent of the course grade.
Lab and Challenge criteria contribute 50 points; system evidence and engineering reasoning
contribute 50 points.

## Part A: Notebook and code criteria (50 points)

Each passed criterion check is worth one point. The criteria apply to the work in your complete
Canvas ZIP; no separate upload is required.

### Lab evaluation notebook (20 points)

| Concept group | Checks | Points |
|---|---:|---:|
| Notebook structure and saved execution | 4 | 4 |
| Evaluation-table integrity | 4 | 4 |
| Confusion matrix and axes | 5 | 5 |
| One-versus-rest metric construction | 4 | 4 |
| Consequence response completed | 3 | 3 |
| **Lab total** | **20** | **20** |

An unexecuted or invalid notebook loses only the affected checks.

### Challenge policy notebook and C++ policy (30 points)

| Concept group | Checks | Points |
|---|---:|---:|
| Operating requirement and threshold selection | 5 | 5 |
| Trace prediction, test design, and policy consistency | 5 | 5 |
| Accept threshold and supplied interface | 10 | 10 |
| Latch, normal release, and repeated windows | 7 | 7 |
| Timeout and rollover behavior | 3 | 3 |
| **Challenge total** | **30** | **30** |

A source-file compile failure loses the C++ behavioral checks but does not erase valid notebook
credit.

## Part B: System behavior, evidence, and reasoning (50 points)

Intermediate scores are allowed when work falls between the anchors.

### B1. Model bring-up and component contract (8 points)

- **0:** No working one-second trial or the model interface is fundamentally misunderstood.
- **4:** Scores run, but the artifact identity, trial evidence, or input/output contract omits important
  fields.
- **6:** Target, non-command, score-vector, file-role, and timing evidence are present with one
  meaningful gap.
- **8:** The pinned and correctly placed artifact, completed model-interface calls, labeled trial, and
  explanation correctly distinguish model metadata from parameters, interface from implementation,
  window duration, stride, processing time, score order, and application events.

### B2. Evaluation interpretation (8 points)

- **0:** Evaluation evidence is absent or unrelated to the released benchmark.
- **4:** A matrix and metrics exist, but axes, test unit, or interpretation are weak.
- **6:** Integrity, matrix, and class metrics are correct with one important interpretation gap.
- **8:** The analysis names a measured confusion, its count, and its timer consequence without
  extending claims beyond the held-out corpus.

### B3. Consequence-based operating policy (10 points)

- **0:** Threshold and temporal values are arbitrary or inconsistent.
- **5:** The supplied requirement and sweep are present, but the selection rule or recall cost is incomplete.
- **8:** The lowest tested threshold meeting the supplied requirement is used consistently, and
  both student-designed traces run, with one evidence or explanation gap.
- **10:** Requirement, selected point, tradeoff, supplied traces, two well-designed student traces,
  normal release, maximum-latch recovery, policy source, README, and serial evidence agree.

### B4. On-device integration and observability (10 points)

- **0:** The model does not control the timer.
- **5:** Some commands work, but mapping, repeated-window suppression, state, or feedback is
  unreliable.
- **8:** The complete sequence works with one meaningful gap in LED, Serial, timer, or rejection
  behavior.
- **10:** Every required command, repeated-utterance suppression, both rearm paths, non-command
  rejection, and inherited A1 timer transition works repeatedly; LEDs and Serial agree with
  accepted events and timer state.

### B5. Local validation and system evidence (8 points)

- **0:** The 40-example local set and integrated-system demonstration are absent.
- **4:** Local or system trials exist, but class counts, conditions, or failure evidence are incomplete.
- **6:** Ten local examples per class and the required system attempts are reported under named conditions with one
  reproducibility gap.
- **8:** The complete local log, local matrix, integrated-system table, demonstration, and observed
  failures agree and remain separate from benchmark claims.

### B6. Timing, reproducibility, and claim discipline (6 points)

- **0:** Build path or timing claims cannot be reproduced.
- **3:** Build settings and timing values are present but sources or limitations are unclear.
- **5:** Exact versions, PSRAM, build steps, configured timing, tool timing, and visible latency are
  distinguished with one gap.
- **6:** Another reviewer can rebuild the package and trace every performance claim to the correct
  benchmark, tool output, or physical observation.
