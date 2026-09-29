extends RefCounted
## Optional diagnostic only. An entry-to-entry interval belongs to the PREVIOUS
## callback plus intervening engine/pacing work, never the current update.
const Metrics = preload("res://frame_metrics.gd")
const THRESHOLD_MS := 1000.0 / 30.0
const FIRST_LIMIT := 16
const WORST_LIMIT := 64
const TRANSITION_LIMIT := 32
const DRAW_GROUPS := ["draw_advanced_focused_drawable", "draw_advanced_other_window_state", "no_draw_advance", "counter_unavailable"]

var wall = Metrics.Series.new(0)
var entries := 0
var completed := 0
var slow_count := 0
var first: Array[Dictionary] = []
var worst: Array[Dictionary] = []
var phase_counts: Dictionary = {}
var current: Dictionary = {}
var previous: Dictionary = {}
var maximum_context: Dictionary = {}
var maximum_body_ms := 0.0
var initialization_ms := 0.0
var start_usec := 0
var mark_usec := 0
var draw_series: Dictionary = {}
var focused_counts := {"all_true": 0, "all_false": 0, "changed": 0, "unavailable": 0}
var drawable_counts := {"all_true": 0, "all_false": 0, "changed": 0, "unavailable": 0}
var drawn_delta_total := 0
var window_observations := 0
var missing_window_observations := 0
var window_transition_count := 0
var window_transitions: Array[Dictionary] = []
var last_window_state: Dictionary = {}

func _init() -> void:
    for group in DRAW_GROUPS: draw_series[group] = Metrics.Series.new(0)

func enter(now_usec: int, context: Dictionary, counters: Dictionary) -> void:
    if entries == 0: start_usec = now_usec
    entries += 1
    observe_window(now_usec, context, "entry")
    if not previous.is_empty() and now_usec > int(previous.begin_usec):
        var interval_ms := (now_usec - int(previous.begin_usec)) / 1000.0
        wall.record(interval_ms)
        var phase: String = previous.entry.phase
        phase_counts[phase] = int(phase_counts.get(phase, 0)) + 1
        var window_interval := classify_window_interval(previous.entry, previous.exit, context)
        draw_series[window_interval.category].record(interval_ms)
        focused_counts[window_interval.focused] += 1
        drawable_counts[window_interval.drawable] += 1
        if window_interval.frames_drawn_delta != null:
            drawn_delta_total += int(window_interval.frames_drawn_delta)
        # Clone only rare slow/max records, never full gameplay snapshots.
        if interval_ms > THRESHOLD_MS or maximum_context.is_empty() or interval_ms > float(maximum_context.wall_ms):
            var row := previous.duplicate(true)
            row.wall_ms = interval_ms
            row.observed_seconds = (now_usec - start_usec) / 1000000.0
            row.outside_measured_callback_ms = maxf(0.0, interval_ms - float(row.body_ms))
            row.next_phase = context.phase
            row.next_window = window_sample(context)
            row.window_interval = window_interval
            row.pipeline_counters_at_next_entry = counters.duplicate(true)
            row.pipeline_delta = counter_delta(previous.pipeline_counters, counters)
            if maximum_context.is_empty() or interval_ms > float(maximum_context.wall_ms):
                maximum_context = row
            if interval_ms > THRESHOLD_MS:
                slow_count += 1
                if first.size() < FIRST_LIMIT: first.append(row)
                if worst.size() < WORST_LIMIT or interval_ms > float(worst[-1].wall_ms):
                    worst.append(row)
                    worst.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
                        return a.wall_ms > b.wall_ms if a.wall_ms != b.wall_ms else a.callback < b.callback)
                    if worst.size() > WORST_LIMIT: worst.resize(WORST_LIMIT)
    current = {"callback": entries, "begin_usec": now_usec,
        "entry": context.duplicate(true), "pipeline_counters": counters.duplicate(true), "segments_ms": {}}
    mark_usec = now_usec

func mark(name: String, now_usec: int) -> void:
    if current.is_empty(): return
    current.segments_ms[name] = maxf(0.0, (now_usec - mark_usec) / 1000.0)
    mark_usec = now_usec

func finish(now_usec: int, context: Dictionary) -> void:
    if current.is_empty(): return
    mark("tail", now_usec)
    current.body_ms = maxf(0.0, (now_usec - int(current.begin_usec)) / 1000.0)
    current.exit = context.duplicate(true)
    observe_window(now_usec, context, "exit")
    current.did_restart = int(context.resets) > int(current.entry.resets)
    maximum_body_ms = maxf(maximum_body_ms, float(current.body_ms))
    previous = current
    current = {}
    completed += 1

static func window_sample(context: Dictionary) -> Dictionary:
    return {"window_focused": context.get("window_focused"),
        "window_drawable": context.get("window_drawable"), "frames_drawn": context.get("frames_drawn")}

static func flag_group(samples: Array, key: String) -> String:
    var true_count := 0
    for sample in samples:
        if typeof(sample.get(key)) != TYPE_BOOL: return "unavailable"
        if sample[key]: true_count += 1
    return "all_true" if true_count == samples.size() else ("all_false" if true_count == 0 else "changed")

static func classify_window_interval(entry: Dictionary, ending: Dictionary, following: Dictionary) -> Dictionary:
    var samples := [entry, ending, following]
    var focused := flag_group(samples, "window_focused")
    var drawable := flag_group(samples, "window_drawable")
    var delta: Variant = null
    var counter_valid := true
    for sample in samples:
        if typeof(sample.get("frames_drawn")) != TYPE_INT or int(sample.frames_drawn) < 0:
            counter_valid = false
    if counter_valid and int(entry.frames_drawn) <= int(ending.frames_drawn) and int(ending.frames_drawn) <= int(following.frames_drawn):
        delta = int(following.frames_drawn) - int(entry.frames_drawn)
    var category := "counter_unavailable"
    if delta != null:
        category = "no_draw_advance" if delta == 0 else ("draw_advanced_focused_drawable" if focused == "all_true" and drawable == "all_true" else "draw_advanced_other_window_state")
    return {"category": category, "focused": focused, "drawable": drawable, "frames_drawn_delta": delta}

func observe_window(now_usec: int, context: Dictionary, boundary: String) -> void:
    window_observations += 1
    var state := {"window_focused": context.get("window_focused"), "window_drawable": context.get("window_drawable")}
    if typeof(state.window_focused) != TYPE_BOOL or typeof(state.window_drawable) != TYPE_BOOL:
        missing_window_observations += 1
    if not last_window_state.is_empty() and state != last_window_state:
        window_transition_count += 1
        if window_transitions.size() < TRANSITION_LIMIT:
            window_transitions.append({"observation": window_observations, "callback": entries,
                "boundary": boundary, "observed_seconds": (now_usec - start_usec) / 1000000.0,
                "before": last_window_state.duplicate(), "after": state.duplicate()})
    last_window_state = state

func window_report() -> Dictionary:
    var categories := {}
    for group in DRAW_GROUPS:
        var series = draw_series[group]
        categories[group] = {"samples": series.count, "p50_ms": series.percentile(.5),
            "p95_ms": series.percentile(.95), "p99_ms": series.percentile(.99),
            "maximum_ms": series.maximum, "slow_count": series.over_30fps}
    return {"schema": "tanks3d-window-render-v1", "counter": "Engine.get_frames_drawn",
        "counter_semantics": "engine_draw_calls_not_present", "interval_samples": wall.count,
        "categories": categories, "focused_interval_counts": focused_counts.duplicate(),
        "drawable_interval_counts": drawable_counts.duplicate(), "frames_drawn_delta_total": drawn_delta_total,
        "observation_count": window_observations, "missing_window_observations": missing_window_observations,
        "transition_count": window_transition_count, "transitions": window_transitions.duplicate(true),
        "limitations": "Entry/exit/next-entry samples only; intermediate window changes can be missed. Engine draw calls are not display presentations or GPU completion. Positive deltas do not prove every callback was presented. Categories include all phases with no warmup exclusion."}

static func counter_delta(before: Dictionary, after: Dictionary) -> Dictionary:
    var result := {}
    for key in before:
        if not after.has(key) or float(after[key]) < float(before[key]):
            result[key] = null # A reset/missing value is unavailable, not zero work.
        else:
            result[key] = float(after[key]) - float(before[key])
    return result

func report() -> Dictionary:
    return {"schema": "tanks3d-slow-frames-v2", "clock": "Time.get_ticks_usec",
        "threshold_ms": THRESHOLD_MS, "callbacks_observed": entries, "callbacks_completed": completed,
        "interval_samples": wall.count, "initialization_ms": initialization_ms,
        "p95_all_phases_ms": wall.percentile(.95), "p99_all_phases_ms": wall.percentile(.99),
        "maximum_all_phases_ms": wall.maximum, "maximum_body_ms": maximum_body_ms,
        "phase_counts": phase_counts.duplicate(), "slow_count": slow_count,
        "first_slow": first.duplicate(true), "worst_slow": worst.duplicate(true),
        "maximum_context": maximum_context.duplicate(true),
        "window_render_diagnostics": window_report(),
        "limitations": "All process intervals after initialization; no warmup exclusion. Prior callback attribution. Counter observations may lag and do not establish causality. Outside-callback time includes rendering, other nodes, deferred work, pacing and diagnostics; it is not GPU-only time. Final report serialization is excluded."}
