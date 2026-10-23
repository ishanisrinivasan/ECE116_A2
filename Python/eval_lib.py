"""Evaluation and sequential-policy helpers for ECE116 A2."""

import csv
from collections import defaultdict

import numpy as np

LABELS = ["go", "up", "stop", "unknown"]
COMMAND_LABELS = LABELS[:3]
SCORE_COLUMNS = [f"score_{label}" for label in LABELS]


def load_evaluation(path):
    """Loads held-out labels, score vectors, and row metadata from one CSV file."""
    rows = []
    with open(path, newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            rows.append(row)
    truth = np.asarray([LABELS.index(row["true_label"]) for row in rows], dtype=np.int64)
    scores = np.asarray([[float(row[column]) for column in SCORE_COLUMNS] for row in rows], dtype=np.float64)
    metadata = {
        "clip_id": [row["clip_id"] for row in rows],
        "source_word": [row["source_word"] for row in rows],
        "speaker_id": [row["speaker_id"] for row in rows],
        "rms": np.asarray([float(row["rms"]) for row in rows]),
    }
    return truth, scores, metadata


def load_local_trials(path):
    """Loads labeled TRIAL_RESULT rows copied from the A2 Lab serial monitor."""
    rows = []
    with open(path, encoding="utf-8") as stream:
        for line in stream:
            if not line.startswith("TRIAL_RESULT "):
                continue
            fields = {}
            for token in line.strip().split()[1:]:
                if "=" in token:
                    key, value = token.split("=", 1)
                    fields[key] = value
            missing = [field for field in ["trial_id", "true_label", "rms", *LABELS] if field not in fields]
            if missing:
                raise ValueError(f"TRIAL_RESULT row is missing: {', '.join(missing)}")
            if fields["true_label"] not in LABELS:
                raise ValueError(f"unknown true label: {fields['true_label']}")
            rows.append(fields)
    if not rows:
        raise ValueError("no TRIAL_RESULT rows were found")

    trial_ids = [int(row["trial_id"]) for row in rows]
    if len(set(trial_ids)) != len(trial_ids):
        raise ValueError("trial IDs must be unique")

    truth = np.asarray([LABELS.index(row["true_label"]) for row in rows], dtype=np.int64)
    scores = np.asarray([[float(row[label]) for label in LABELS] for row in rows], dtype=np.float64)
    metadata = {
        "trial_id": trial_ids,
        "rms": np.asarray([float(row["rms"]) for row in rows]),
    }
    return truth, scores, metadata


def threshold_sweep(truth, scores, thresholds=None):
    """Measures command precision and recall at each candidate accept threshold."""
    if thresholds is None:
        thresholds = np.linspace(0.0, 0.95, 20)
    prediction = scores.argmax(axis=1)
    top_score = scores.max(axis=1)
    true_commands = truth < len(COMMAND_LABELS)
    rows = []
    for threshold in thresholds:
        accepts = (prediction < len(COMMAND_LABELS)) & (top_score >= threshold)
        correct = accepts & (prediction == truth) & true_commands
        rows.append({
            "threshold": float(threshold),
            "accepts": int(accepts.sum()),
            "correct_accepts": int(correct.sum()),
            "false_accepts": int((accepts & ~correct).sum()),
            "precision": float(correct.sum() / max(accepts.sum(), 1)),
            "recall": float(correct.sum() / max(true_commands.sum(), 1)),
        })
    return rows


def lowest_threshold_meeting(sweep, minimum_precision):
    """Returns the lowest tested operating point meeting a precision requirement."""
    candidates = [row for row in sweep if row["precision"] >= minimum_precision]
    return min(candidates, key=lambda row: row["threshold"]) if candidates else None


def load_traces(path):
    """Groups timestamped score vectors by sequential trace identifier."""
    traces = defaultdict(list)
    with open(path, newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            traces[row["trace_id"]].append({
                "time_ms": int(row["time_ms"]),
                "scores": np.asarray([float(row[column]) for column in SCORE_COLUMNS]),
            })
    return dict(traces)


def load_trace_manifest(path):
    """Loads the expected command and event count for each supplied trace."""
    with open(path, newline="", encoding="utf-8") as stream:
        return {row["trace_id"]: {
            "expected_command": row["expected_command"],
            "expected_event_count": int(row["expected_event_count"]),
        } for row in csv.DictReader(stream)}


def trace_policy(rows, accept_threshold, release_threshold=0.55,
                 minimum_latch_ms=1000, maximum_latch_ms=1500):
    """Returns every intermediate decision made while replaying one score trace."""
    timeline = []
    command_is_latched = False
    fired_at = 0
    for row in rows:
        scores = row["scores"]
        now = row["time_ms"]
        top = int(scores.argmax())
        state_before = "LATCHED" if command_is_latched else "ARMED"
        event = "none"
        normal_release = False
        maximum_timeout = False
        reason = "rejected window"
        if command_is_latched:
            every_target_below_release = bool(np.all(scores[:len(COMMAND_LABELS)] < release_threshold))
            minimum_elapsed = now - fired_at >= minimum_latch_ms
            normal_release = every_target_below_release and minimum_elapsed
            maximum_timeout = now - fired_at >= maximum_latch_ms
            if normal_release or maximum_timeout:
                command_is_latched = False
                reason = "normal release" if normal_release else "maximum-latch recovery"
            else:
                reason = "suppressed while latched"
        elif top < len(COMMAND_LABELS) and scores[top] >= accept_threshold:
            command_is_latched = True
            fired_at = now
            event = COMMAND_LABELS[top]
            reason = "accepted target"
        timeline.append({
            "time_ms": now,
            "state_before": state_before,
            "top_label": LABELS[top],
            "top_score": float(scores[top]),
            "normal_release": normal_release,
            "maximum_timeout": maximum_timeout,
            "event": event,
            "state_after": "LATCHED" if command_is_latched else "ARMED",
            "reason": reason,
        })
    return timeline


def replay_policy(rows, accept_threshold, release_threshold=0.55,
                  minimum_latch_ms=1000, maximum_latch_ms=1500):
    """Replays the supplied release-or-timeout policy over one ordered score trace."""
    timeline = trace_policy(
        rows, accept_threshold, release_threshold, minimum_latch_ms, maximum_latch_ms
    )
    events = [
        {"time_ms": row["time_ms"], "command": row["event"], "score": row["top_score"]}
        for row in timeline if row["event"] != "none"
    ]
    return events


def make_trace_window(time_ms, go, up, stop, unknown):
    """Creates one editable score-trace row for a student-designed policy test."""
    return {
        "time_ms": int(time_ms),
        "scores": np.asarray([go, up, stop, unknown], dtype=np.float64),
    }


def replay_report(traces, manifest, accept_threshold, release_threshold=0.55,
                  minimum_latch_ms=1000, maximum_latch_ms=1500):
    """Compares replayed command events with the expected result for every trace."""
    report = []
    for trace_id, rows in traces.items():
        events = replay_policy(
            rows, accept_threshold, release_threshold, minimum_latch_ms, maximum_latch_ms
        )
        expected = manifest[trace_id]
        report.append({
            "trace_id": trace_id,
            "expected": f"{expected['expected_event_count']} x {expected['expected_command']}",
            "observed": ", ".join(event["command"] for event in events) or "none",
            "event_count": len(events),
            "pass": len(events) == expected["expected_event_count"] and (
                expected["expected_command"] == "none" or
                all(event["command"] == expected["expected_command"] for event in events)
            ),
        })
    return report
