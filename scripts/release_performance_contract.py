#!/usr/bin/env python3
"""Shared validation for candidate-generated raw performance telemetry.

This module deliberately validates evidence integrity rather than Alpha release
acceptance.  A structurally honest short or below-threshold run may receive a
runner receipt; the final release verifier separately enforces the 30-minute,
FPS, focus, gameplay, memory, and completed-stage policy.
"""

import datetime
from dataclasses import dataclass
import math
import re
from typing import Any, Mapping, Optional, Sequence, Set


PERFORMANCE_LOG_V2_SCHEMA = "tanks3d-performance-log-v2"
PERFORMANCE_LOG_V2_KEYS = (
    "schema",
    "producer",
    "source_commit",
    "source_tag",
    "candidate_sha256",
    "session_nonce",
    "started_at_utc",
    "completed_at_utc",
    "monotonic_duration_us",
    "target_interval_us",
    "clock",
    "memory_metric",
    "memory_unit",
    "clean_shutdown",
    "samples",
)
PERFORMANCE_SAMPLE_V2_KEYS = (
    "sequence",
    "elapsed_us",
    "window_duration_us",
    "rendered_frames",
    "resident_bytes",
    "gameplay_duration_us",
    "focused_duration_us",
    "stage_clear_events",
    "completed_stages",
    "stage_number",
    "player_count",
    "app_state",
    "window_focused",
)
PERFORMANCE_V2_APP_STATES = ("gameplay", "settlement", "high_score")
PERFORMANCE_V2_PRODUCER = "Tanks3D"
PERFORMANCE_V2_CLOCK = "steady_clock"
PERFORMANCE_V2_MEMORY_METRIC = "proc_pid_rusage.ri_phys_footprint"
PERFORMANCE_V2_MEMORY_UNIT = "bytes"
PERFORMANCE_V2_TARGET_INTERVAL_US = 1_000_000
PERFORMANCE_V2_MINIMUM_WINDOW_US = 750_000
PERFORMANCE_V2_MAXIMUM_WINDOW_US = 1_250_000
PERFORMANCE_V2_MAXIMUM_DURATION_SECONDS = 4 * 60 * 60
PERFORMANCE_V2_START_MARKER = "TANKS3D_PERFORMANCE_START"
PERFORMANCE_V2_COMPLETE_MARKER = "TANKS3D_PERFORMANCE_COMPLETE"
MAX_TELEMETRY_BYTES = 32 * 1024 * 1024
PERFORMANCE_ARTIFACT_MAXIMUM_BYTES = {
    "receipt": 64 * 1024,
    "telemetry": MAX_TELEMETRY_BYTES,
    "stdout": 16 * 1024 * 1024,
    "stderr": 0,
}
MAX_UINT64 = (1 << 64) - 1
UTC_RE = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$")


class PerformanceContractError(ValueError):
    """Raw telemetry or marker data violates the shared v2 contract."""


@dataclass(frozen=True)
class RunBinding:
    source_commit: str
    source_tag: str
    candidate_sha256: str
    session_nonce: str
    requested_duration_seconds: int
    receipt_started_at_utc: str
    receipt_completed_at_utc: str
    expected_started_at_utc: Optional[str] = None
    expected_completed_at_utc: Optional[str] = None


@dataclass(frozen=True)
class PerformanceSummary:
    started_at_utc: str
    completed_at_utc: str
    wall_duration_us: int
    monotonic_duration_us: int
    sample_count: int
    completed_stages: int
    stage_clear_events: int
    average_fps: float
    minimum_fps: float
    one_percent_low_fps: float
    memory_start_mb: float
    memory_end_mb: float
    memory_growth_bytes: int
    gameplay_duration_ratio: float
    focused_duration_ratio: float


def _require_object(value: Any, context: str) -> Mapping[str, Any]:
    if not isinstance(value, dict):
        raise PerformanceContractError("{} must be an object".format(context))
    return value


def _require_array(value: Any, context: str) -> Sequence[Any]:
    if not isinstance(value, list):
        raise PerformanceContractError("{} must be an array".format(context))
    return value


def _require_string(value: Any, context: str) -> str:
    if not isinstance(value, str):
        raise PerformanceContractError("{} must be a string".format(context))
    return value


def _require_exact_keys(
    value: Mapping[str, Any], expected: Set[str], context: str
) -> None:
    actual = set(value)
    if actual == expected:
        return
    missing = sorted(expected - actual)
    extra = sorted(actual - expected)
    details = []
    if missing:
        details.append("missing {}".format(", ".join(missing)))
    if extra:
        details.append("unexpected {}".format(", ".join(extra)))
    raise PerformanceContractError(
        "{} has invalid keys ({})".format(context, "; ".join(details))
    )


def _require_json_integer(value: Any, minimum: int, context: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise PerformanceContractError("{} must be a raw JSON integer".format(context))
    if value < minimum:
        raise PerformanceContractError(
            "{} must be at least {}".format(context, minimum)
        )
    return value


def _require_timestamp(value: Any, context: str) -> str:
    timestamp = _require_string(value, context)
    if UTC_RE.fullmatch(timestamp) is None:
        raise PerformanceContractError(
            "{} must use YYYY-MM-DDTHH:MM:SSZ".format(context)
        )
    try:
        parsed = datetime.datetime.strptime(timestamp, "%Y-%m-%dT%H:%M:%SZ").replace(
            tzinfo=datetime.timezone.utc
        )
    except ValueError as exc:
        raise PerformanceContractError(
            "{} is not a real UTC timestamp: {}".format(context, exc)
        )
    if parsed > datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(
        minutes=5
    ):
        raise PerformanceContractError("{} must not be in the future".format(context))
    return timestamp


def _timestamp_value(value: str) -> datetime.datetime:
    return datetime.datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(
        tzinfo=datetime.timezone.utc
    )


def validate_duration_seconds(value: Any) -> int:
    """Validate a recorder-supported duration, not the final Alpha minimum."""

    if isinstance(value, bool) or not isinstance(value, int):
        raise PerformanceContractError("duration seconds must be an integer")
    if value < 1 or value > PERFORMANCE_V2_MAXIMUM_DURATION_SECONDS:
        raise PerformanceContractError(
            "duration seconds must be between 1 and {}".format(
                PERFORMANCE_V2_MAXIMUM_DURATION_SECONDS
            )
        )
    return value


def validate_performance_markers(stdout_text: str, nonce: str) -> None:
    start = "{} {}".format(PERFORMANCE_V2_START_MARKER, nonce)
    complete = "{} {}".format(PERFORMANCE_V2_COMPLETE_MARKER, nonce)
    marker_lines = [
        line
        for line in stdout_text.splitlines()
        if line.startswith(PERFORMANCE_V2_START_MARKER)
        or line.startswith(PERFORMANCE_V2_COMPLETE_MARKER)
    ]
    if marker_lines != [start, complete]:
        raise PerformanceContractError(
            "performance stdout must contain exactly one ordered matching "
            "START/COMPLETE marker"
        )


def validate_performance_log_v2(
    payload: Any, binding: RunBinding
) -> PerformanceSummary:
    """Validate raw v2 structure and return derived, policy-neutral metrics."""

    duration_seconds = validate_duration_seconds(binding.requested_duration_seconds)
    log = _require_object(payload, "performance log v2")
    _require_exact_keys(log, set(PERFORMANCE_LOG_V2_KEYS), "performance log v2")
    expected_strings = {
        "schema": PERFORMANCE_LOG_V2_SCHEMA,
        "producer": PERFORMANCE_V2_PRODUCER,
        "source_commit": binding.source_commit,
        "source_tag": binding.source_tag,
        "candidate_sha256": binding.candidate_sha256,
        "session_nonce": binding.session_nonce,
        "clock": PERFORMANCE_V2_CLOCK,
        "memory_metric": PERFORMANCE_V2_MEMORY_METRIC,
        "memory_unit": PERFORMANCE_V2_MEMORY_UNIT,
    }
    for key, expected in expected_strings.items():
        if _require_string(log[key], "performance log v2." + key) != expected:
            raise PerformanceContractError(
                "performance log v2 {} does not match its candidate receipt".format(
                    key
                )
            )
    if log["clean_shutdown"] is not True:
        raise PerformanceContractError(
            "performance log v2 requires clean_shutdown=true"
        )

    receipt_started_text = _require_timestamp(
        binding.receipt_started_at_utc, "performance QA receipt.started_at_utc"
    )
    receipt_completed_text = _require_timestamp(
        binding.receipt_completed_at_utc, "performance QA receipt.completed_at_utc"
    )
    receipt_started = _timestamp_value(receipt_started_text)
    receipt_completed = _timestamp_value(receipt_completed_text)
    if receipt_completed < receipt_started:
        raise PerformanceContractError("performance QA receipt ends before it starts")

    started_text = _require_timestamp(
        log["started_at_utc"], "performance log v2.started_at_utc"
    )
    completed_text = _require_timestamp(
        log["completed_at_utc"], "performance log v2.completed_at_utc"
    )
    started = _timestamp_value(started_text)
    completed = _timestamp_value(completed_text)
    expected_interval = (
        binding.expected_started_at_utc,
        binding.expected_completed_at_utc,
    )
    if (expected_interval[0] is None) != (expected_interval[1] is None):
        raise PerformanceContractError(
            "performance log expected interval binding is incomplete"
        )
    if expected_interval[0] is not None and (
        started_text != expected_interval[0]
        or completed_text != expected_interval[1]
    ):
        raise PerformanceContractError(
            "performance log interval does not match its interactive session"
        )
    if not (receipt_started <= started <= completed <= receipt_completed):
        raise PerformanceContractError(
            "performance receipt and telemetry timestamps are not nested chronologically"
        )

    monotonic_duration = _require_json_integer(
        log["monotonic_duration_us"],
        1,
        "performance log v2.monotonic_duration_us",
    )
    target_interval = _require_json_integer(
        log["target_interval_us"], 1, "performance log v2.target_interval_us"
    )
    if target_interval != PERFORMANCE_V2_TARGET_INTERVAL_US:
        raise PerformanceContractError(
            "performance log v2 target interval is not canonical"
        )
    requested_duration_us = duration_seconds * 1_000_000
    if not (
        requested_duration_us
        <= monotonic_duration
        <= requested_duration_us + PERFORMANCE_V2_MAXIMUM_WINDOW_US
    ):
        raise PerformanceContractError(
            "performance log v2 duration does not match the runner request"
        )
    wall_duration_us = int((completed - started).total_seconds() * 1_000_000)
    if abs(wall_duration_us - monotonic_duration) > 2 * target_interval:
        raise PerformanceContractError(
            "performance log v2 wall and monotonic durations disagree"
        )

    samples = _require_array(log["samples"], "performance log v2.samples")
    if not samples:
        raise PerformanceContractError("performance log v2 has no raw samples")
    previous_elapsed = 0
    previous_completed_stages = 0
    total_frames = 0
    total_window_duration = 0
    total_gameplay_duration = 0
    total_focused_duration = 0
    total_stage_clear_events = 0
    fps_values = []
    memory_values = []
    for index, raw_sample in enumerate(samples):
        context = "performance log v2.samples[{}]".format(index)
        sample = _require_object(raw_sample, context)
        _require_exact_keys(sample, set(PERFORMANCE_SAMPLE_V2_KEYS), context)
        sequence = _require_json_integer(sample["sequence"], 1, context + ".sequence")
        if sequence != index + 1:
            raise PerformanceContractError(
                "performance log v2 sample sequence must be contiguous from 1"
            )
        elapsed = _require_json_integer(sample["elapsed_us"], 1, context + ".elapsed_us")
        window = _require_json_integer(
            sample["window_duration_us"], 1, context + ".window_duration_us"
        )
        if not (
            PERFORMANCE_V2_MINIMUM_WINDOW_US
            <= window
            <= PERFORMANCE_V2_MAXIMUM_WINDOW_US
        ):
            raise PerformanceContractError(
                "performance log v2 sample window is outside 0.75-1.25 seconds"
            )
        if elapsed != previous_elapsed + window:
            raise PerformanceContractError(
                "performance log v2 samples are missing or contain fabricated catch-up"
            )
        frames = _require_json_integer(
            sample["rendered_frames"], 1, context + ".rendered_frames"
        )
        resident = _require_json_integer(
            sample["resident_bytes"], 1, context + ".resident_bytes"
        )
        if resident > MAX_UINT64:
            raise PerformanceContractError(
                context + ".resident_bytes must fit an unsigned 64-bit integer"
            )
        gameplay_duration = _require_json_integer(
            sample["gameplay_duration_us"],
            0,
            context + ".gameplay_duration_us",
        )
        focused_duration = _require_json_integer(
            sample["focused_duration_us"],
            0,
            context + ".focused_duration_us",
        )
        if gameplay_duration > window or focused_duration > window:
            raise PerformanceContractError(
                context + " records gameplay/focus duration beyond its sample window"
            )
        stage_clear_events = _require_json_integer(
            sample["stage_clear_events"], 0, context + ".stage_clear_events"
        )
        if stage_clear_events > 1:
            raise PerformanceContractError(
                context + " records more than one cleared stage in one sample window"
            )
        completed_stages = _require_json_integer(
            sample["completed_stages"], 0, context + ".completed_stages"
        )
        if completed_stages != previous_completed_stages + stage_clear_events:
            raise PerformanceContractError(
                "performance log v2 completed_stages contradicts its clear events"
            )
        _require_json_integer(sample["stage_number"], 1, context + ".stage_number")
        player_count = _require_json_integer(
            sample["player_count"], 1, context + ".player_count"
        )
        if player_count != 1:
            raise PerformanceContractError(
                "performance log v2 --quick-start receipt requires one-player samples"
            )
        app_state = _require_string(sample["app_state"], context + ".app_state")
        if app_state not in PERFORMANCE_V2_APP_STATES:
            raise PerformanceContractError(
                "performance log v2 sample app_state is not canonical"
            )
        if not isinstance(sample["window_focused"], bool):
            raise PerformanceContractError(
                "{}.window_focused must be a JSON boolean".format(context)
            )
        if app_state == "gameplay" and gameplay_duration == 0:
            raise PerformanceContractError(
                context + " gameplay duration contradicts its app_state"
            )
        if app_state != "gameplay" and gameplay_duration == window:
            raise PerformanceContractError(
                context + " non-gameplay state contradicts a full gameplay window"
            )
        if stage_clear_events > 0 and gameplay_duration == window:
            raise PerformanceContractError(
                context + " clear event lacks a rendered settlement frame"
            )
        if sample["window_focused"] and focused_duration == 0:
            raise PerformanceContractError(
                context + " focused endpoint contradicts zero focused duration"
            )
        if not sample["window_focused"] and focused_duration == window:
            raise PerformanceContractError(
                context + " unfocused endpoint contradicts a fully focused window"
            )
        if frames * 1_000_000 > 1000 * window:
            raise PerformanceContractError(
                context + " records an implausible FPS value"
            )
        fps = frames * 1_000_000.0 / window
        previous_elapsed = elapsed
        previous_completed_stages = completed_stages
        total_frames += frames
        total_window_duration += window
        total_gameplay_duration += gameplay_duration
        total_focused_duration += focused_duration
        total_stage_clear_events += stage_clear_events
        fps_values.append(fps)
        memory_values.append(resident)
    if (
        previous_elapsed != monotonic_duration
        or total_window_duration != monotonic_duration
    ):
        raise PerformanceContractError(
            "performance log v2 has insufficient raw sampling coverage or an "
            "inconsistent monotonic duration"
        )

    low_count = max(1, math.ceil(len(fps_values) * 0.01))
    return PerformanceSummary(
        started_at_utc=started_text,
        completed_at_utc=completed_text,
        wall_duration_us=wall_duration_us,
        monotonic_duration_us=monotonic_duration,
        sample_count=len(samples),
        completed_stages=previous_completed_stages,
        stage_clear_events=total_stage_clear_events,
        average_fps=total_frames * 1_000_000.0 / total_window_duration,
        minimum_fps=min(fps_values),
        one_percent_low_fps=(
            sum(sorted(fps_values)[:low_count]) / low_count
        ),
        memory_start_mb=memory_values[0] / 1048576.0,
        memory_end_mb=memory_values[-1] / 1048576.0,
        memory_growth_bytes=max(memory_values) - memory_values[0],
        gameplay_duration_ratio=(
            total_gameplay_duration / total_window_duration
        ),
        focused_duration_ratio=(
            total_focused_duration / total_window_duration
        ),
    )
