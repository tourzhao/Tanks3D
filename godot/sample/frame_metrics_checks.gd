extends SceneTree
const Metrics = preload("res://frame_metrics.gd")

func _initialize() -> void:
    var metrics = Metrics.new()
    # Warmup spikes must not contaminate the recorded percentiles.
    for index in 120: metrics.record(900.0, 300.0, 900.0, index / 60.0)
    # More samples than the old 108,000-element history limit, with known
    # 60-second windows. The full run must remain represented in bounded bins.
    for index in 144000:
        metrics.record(16.666, 2.125, 16.666, 2.0 + index / 120.0)
    var report: Dictionary = metrics.report(1202.0)
    var valid: bool = report.timing_samples == 144000 and report.wall_timing_samples == 144000
    valid = valid and report.p95_wall_frame_ms >= 16.666 and report.p95_wall_frame_ms <= 16.676
    valid = valid and report.p95_update_ms >= 2.125 and report.p95_update_ms <= 2.135
    valid = valid and report.wall_over_16_667_ms == 0 and report.timing_windows.size() == 21
    valid = valid and metrics.wall.bins.size() == Metrics.LAST_BIN + 1
    valid = valid and metrics.window_wall.is_empty() and metrics.window_updates.is_empty()
    var overflow = Metrics.Series.new()
    for index in 120: overflow.record(1.0)
    overflow.record(700.0)
    overflow.record(950.0)
    overflow.record(NAN)
    overflow.record(-1.0)
    valid = valid and overflow.count == 2 and overflow.percentile(.99) == 950.0
    if not valid:
        push_error("Bounded timing collection lost samples, warmup, precision or overflow accounting")
        quit(1)
        return
    print("TANKS_FRAME_METRICS_PASSED warmup/full-run/windows/bounds/overflow")
    quit()
