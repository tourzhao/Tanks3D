extends RefCounted
## Bounded diagnostic timing storage. Histogram percentiles round upward by at
## most 0.01 ms below 500 ms; an overflowing percentile reports the real maximum.
## No samples are discarded after warmup, even during a long soak run.

const WARMUP := 120
const BIN_MS := 0.01
const LAST_BIN := 50001

class Series:
    var bins := PackedInt64Array()
    var seen := 0
    var count := 0
    var maximum := 0.0
    var over_60fps := 0
    var over_30fps := 0
    var warmup := WARMUP

    func _init(skip_samples: int = WARMUP) -> void:
        warmup = maxi(0, skip_samples)
        bins.resize(LAST_BIN + 1)

    func record(value: float) -> bool:
        if not is_finite(value) or value <= 0.0:
            return false
        seen += 1
        if seen <= warmup:
            return false
        bins[mini(LAST_BIN, ceili(value / BIN_MS))] += 1
        count += 1
        maximum = maxf(maximum, value)
        if value > 1000.0 / 60.0: over_60fps += 1
        if value > 1000.0 / 30.0: over_30fps += 1
        return true

    func percentile(fraction: float) -> float:
        if count == 0: return 0.0
        var rank := mini(count - 1, int(count * fraction))
        var total := 0
        for index in bins.size():
            total += bins[index]
            if total > rank:
                return maximum if index == LAST_BIN else index * BIN_MS
        return maximum

var engine := Series.new()
var update := Series.new()
var wall := Series.new()
var windows: Array[Dictionary] = []
var window_wall: Array[float] = []
var window_updates: Array[float] = []
var window_start := 0.0

func record(delta_ms: float, update_ms: float, wall_ms: float, seconds: float) -> void:
    engine.record(delta_ms)
    if update.record(update_ms): window_updates.append(update_ms)
    if wall.record(wall_ms): window_wall.append(wall_ms)
    if seconds - window_start >= 60.0:
        finish_window(seconds)

func finish_window(seconds: float) -> void:
    if not window_wall.is_empty():
        window_wall.sort()
        window_updates.sort()
        windows.append({"start_seconds": window_start, "end_seconds": seconds,
            "samples": window_wall.size(),
            "p50_wall_ms": exact_percentile(window_wall, .5),
            "p95_wall_ms": exact_percentile(window_wall, .95),
            "p99_wall_ms": exact_percentile(window_wall, .99),
            "p95_update_ms": exact_percentile(window_updates, .95)})
    window_wall.clear()
    window_updates.clear()
    window_start = seconds

static func exact_percentile(values: Array[float], fraction: float) -> float:
    return values[mini(values.size() - 1, int(values.size() * fraction))] if not values.is_empty() else 0.0

func report(seconds: float) -> Dictionary:
    finish_window(seconds)
    return {"p50_frame_ms": engine.percentile(.5), "p95_frame_ms": engine.percentile(.95),
        "p99_frame_ms": engine.percentile(.99), "p95_update_ms": update.percentile(.95),
        "p50_wall_frame_ms": wall.percentile(.5), "p95_wall_frame_ms": wall.percentile(.95),
        "p99_wall_frame_ms": wall.percentile(.99), "wall_timing_samples": wall.count,
        "timing_samples": engine.count, "timing_windows": windows,
        "wall_over_16_667_ms": wall.over_60fps, "wall_over_33_333_ms": wall.over_30fps,
        "maximum_wall_ms": wall.maximum, "timing_histogram_bin_ms": BIN_MS,
        "timing_histogram_overflow_samples": wall.bins[LAST_BIN]}
