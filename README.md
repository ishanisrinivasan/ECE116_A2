# Voice-Controlled Embedded Timer

A countdown timer you control by voice, running entirely on a Seeed Studio XIAO ESP32-S3 Sense
microcontroller. Say "up" to add 10 seconds, "go" to start the countdown, and "stop" to cancel.
Three LEDs show the timer state. Built for UCSD ECE 116 (Fall 2026).

**Demo:** [voice_timer_demo.mp4](Documentation/Fig/voice_timer_demo.mp4) shows the LEDs and the
Serial Monitor while voice commands drive the timer.

## How it works

```text
PDM microphone -> 1 s audio window every 250 ms -> keyword model (go / up / stop / unknown)
-> command policy (accept + latch/rearm) -> timer event -> LED countdown timer
```

1. **Audio:** the onboard PDM microphone records at 16 kHz. A new one-second window is ready every
   250 ms.
2. **Keyword model:** a 4-class model (512 log-mel features -> 64 ReLU -> softmax) runs on the
   board in about 300 ms per window.
3. **Command policy** (`Arduino/Challenge_2/command_policy.cpp`): it accepts a command only when
   its score is at least 0.90. Then it latches, so one spoken word produces exactly one event. It
   rearms once all command scores drop below 0.55 for at least 1 s, or after a 1.5 s timeout.
4. **Timer:** a state machine that goes STANDBY -> ADDING -> COUNTDOWN -> DONE. The green, amber,
   and red LEDs show which state it's in.

## Results

- **Benchmark** (623 held-out clips, 236 speakers): 83.6% argmax accuracy. The 0.90 threshold
  gives 97.3% command precision at 55.0% recall.
- **Local validation** (40 one-second trials, one speaker): 57.5% accuracy. Under these conditions
  the model over-predicts "go".
- **On hardware:** 15 of 15 commands were correct across five up -> go -> stop cycles, and 10
  non-command phrases produced no false events.
- **Latency:** about 1.0 s from the start of a spoken command to the LED change, measured on video.

The full report, with methods, logs, and limitations, is in
[Documentation/README.md](Documentation/README.md).

## Repository layout

| Path | Contents |
|---|---|
| `Arduino/Lab_2_1/` | Sketch that records one labeled one-second trial and prints the model scores |
| `Arduino/Challenge_2/` | Full voice timer: command policy plus timer logic |
| `Arduino/*/src/ECE116_A2_Keyword/` | Keyword model and audio capture (release v1.0.0) |
| `Python/Lab_2_2_Evaluation.ipynb` | Benchmark and local evaluation: confusion matrices and per-class metrics |
| `Python/Challenge_2_Policy.ipynb` | Threshold sweep and policy trace tests |
| `Python/check_command_policy.py` | Host-side C++ test of the command policy |
| `Documentation/` | Report, figures, serial logs, and demo video |

## Build

1. Install Arduino IDE 2.x with the `esp32` core 3.3.10. Select board `XIAO_ESP32S3` and set
   Tools > PSRAM > OPI PSRAM.
2. Wire the LEDs: green on D1, amber on D2, and red on D3, each through a current-limiting
   resistor to GND.
3. Open `Arduino/Challenge_2/`, upload it, and open Serial Monitor at 115200 baud.
4. To run the policy self-check on your computer: `cd Python && python check_command_policy.py`

**Tech:** C++ (Arduino, ESP32-S3), Python (NumPy, Matplotlib, Jupyter), on-device ML inference.

