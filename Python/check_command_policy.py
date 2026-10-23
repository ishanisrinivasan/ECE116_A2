#!/usr/bin/env python3
"""Compile and run the public A2 command-policy checks on this computer."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


PACKAGE = Path(__file__).resolve().parents[1]
POLICY = PACKAGE / "Arduino" / "Challenge_2"

MODEL_STUB = """\
#ifndef ECE116_KEYWORD_H
#define ECE116_KEYWORD_H
#define EI_CLASSIFIER_LABEL_COUNT 4
struct ei_impulse_result_classification_t { const char *label; float value; };
#endif
"""

HARNESS = r"""
#include <array>
#include <cstdint>
#include <iostream>

#include "command_policy.h"

using Scores = std::array<ei_impulse_result_classification_t, 4>;

Scores scores(float go, float up, float stop, float unknown) {
  return {{{"go", go}, {"up", up}, {"stop", stop}, {"unknown", unknown}}};
}

int passed = 0;
int total = 0;

void check(bool condition, const char *name) {
  ++total;
  if (condition) {
    ++passed;
    std::cout << "PASS " << name << '\n';
  } else {
    std::cout << "FAIL " << name << '\n';
  }
}

void blocked(const char *name, const char *reason) {
  ++total;
  std::cout << "BLOCKED " << name << " -- " << reason << '\n';
}

CommandPolicyState freshState(uint32_t nowMs = 0) {
  CommandPolicyState state{};
  resetCommandPolicy(&state, nowMs);
  return state;
}

int main() {
  CommandPolicyConfig config = makeCommandPolicyConfig();
  check(config.acceptThreshold > 0.0f && config.acceptThreshold <= 1.0f
            && config.releaseThreshold >= 0.0f
            && config.releaseThreshold < config.acceptThreshold
            && config.minimumLatchMs > 0
            && config.maximumLatchMs > config.minimumLatchMs,
        "policy values");

  check(commandEventForClassIndex(0) == COMMAND_EVENT_GO
            && commandEventForClassIndex(1) == COMMAND_EVENT_UP
            && commandEventForClassIndex(2) == COMMAND_EVENT_STOP
            && commandEventForClassIndex(3) == COMMAND_EVENT_NONE,
        "class-to-event mapping");

  CommandPolicyState state = freshState();
  auto go = scores(0.96f, 0.01f, 0.01f, 0.02f);
  auto up = scores(0.01f, 0.96f, 0.01f, 0.02f);
  auto stop = scores(0.01f, 0.01f, 0.96f, 0.02f);
  auto quiet = scores(0.10f, 0.20f, 0.10f, 0.60f);
  auto rejected = scores(config.acceptThreshold - 0.01f, 0.02f, 0.01f, 0.08f);

  check(updateCommandPolicy(&state, rejected.data(), 100, config) == COMMAND_EVENT_NONE,
        "sub-threshold rejection");
  bool accepted = updateCommandPolicy(&state, go.data(), 200, config) == COMMAND_EVENT_GO;
  check(accepted, "accepted command");
  if (accepted) {
    check(updateCommandPolicy(&state, up.data(), 450, config) == COMMAND_EVENT_NONE
              && updateCommandPolicy(&state, stop.data(), 700, config) == COMMAND_EVENT_NONE,
          "class-shift suppression");

    updateCommandPolicy(&state, quiet.data(), 200 + config.minimumLatchMs - 1, config);
    check(state.commandIsLatched, "minimum latch time");
    check(updateCommandPolicy(&state, quiet.data(), 200 + config.minimumLatchMs, config)
              == COMMAND_EVENT_NONE && !state.commandIsLatched,
          "quiet release window");
    check(updateCommandPolicy(&state, up.data(), 450 + config.minimumLatchMs, config)
              == COMMAND_EVENT_UP,
          "later utterance after release");
  } else {
    blocked("class-shift suppression", "accepted command did not establish a latch");
    blocked("minimum latch time", "accepted command did not establish a latch");
    blocked("quiet release window", "accepted command did not establish a latch");
    blocked("later utterance after release", "normal release could not be exercised");
  }

  state = freshState();
  bool timeoutAccepted = updateCommandPolicy(&state, go.data(), 200, config) == COMMAND_EVENT_GO;
  if (timeoutAccepted) {
    check(updateCommandPolicy(&state, go.data(), 200 + config.maximumLatchMs - 1, config)
              == COMMAND_EVENT_NONE && state.commandIsLatched,
          "maximum latch not early");
    check(updateCommandPolicy(&state, go.data(), 200 + config.maximumLatchMs, config)
              == COMMAND_EVENT_NONE && !state.commandIsLatched,
          "maximum latch timeout window");
    check(updateCommandPolicy(&state, up.data(), 450 + config.maximumLatchMs, config)
              == COMMAND_EVENT_UP,
          "later utterance after timeout");
  } else {
    blocked("maximum latch not early", "accepted command did not establish a latch");
    blocked("maximum latch timeout window", "accepted command did not establish a latch");
    blocked("later utterance after timeout", "timeout recovery could not be exercised");
  }

  std::cout << "Public command-policy checks: " << passed << '/' << total << '\n';
  return passed == total ? 0 : 1;
}
"""


def find_compiler():
    """Returns the first supported host C++ compiler available on PATH."""
    return next(
        (shutil.which(name) for name in ("c++", "g++", "clang++") if shutil.which(name)),
        None,
    )


def main():
    compiler = find_compiler()
    if sys.argv[1:] == ["--check-environment"]:
        if compiler is None:
            print("policy-check compiler: NOT FOUND")
            print("Use a Python environment that provides c++, g++, or clang++, such as Google Colab.")
            raise SystemExit(1)
        print(f"policy-check compiler: {compiler}")
        print("This host compiler is used only for the isolated policy self-check.")
        print("Arduino IDE uses its own ESP32 compiler for the complete board sketch.")
        return
    if sys.argv[1:]:
        raise SystemExit("Usage: python check_command_policy.py [--check-environment]")
    if compiler is None:
        raise SystemExit("No C++ compiler found. Install the platform compiler or run this cell in Colab.")

    with tempfile.TemporaryDirectory(prefix="ece116-a2-policy-") as temporary_name:
        work = Path(temporary_name)
        model_stub = work / "src" / "ECE116_A2_Keyword"
        model_stub.mkdir(parents=True)
        (model_stub / "ece116_keyword.h").write_text(MODEL_STUB, encoding="utf-8")
        shutil.copy2(POLICY / "command_policy.h", work / "command_policy.h")
        shutil.copy2(POLICY / "command_policy.cpp", work / "command_policy.cpp")
        (work / "policy_check.cpp").write_text(HARNESS, encoding="utf-8")

        binary = work / "policy_check"
        compile_result = subprocess.run(
            [compiler, "-std=c++17", "-Wall", "-Wextra", "-pedantic", "-I", str(work),
             str(work / "command_policy.cpp"), str(work / "policy_check.cpp"), "-o", str(binary)],
            text=True,
            capture_output=True,
        )
        if compile_result.returncode != 0:
            sys.stdout.write(compile_result.stdout)
            sys.stderr.write(compile_result.stderr)
            raise SystemExit("Command policy did not compile.")

        result = subprocess.run([str(binary)], text=True)
        raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
