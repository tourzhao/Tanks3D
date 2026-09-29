extends SceneTree
const Trace = preload("res://frame_trace.gd")
var valid := true

func context(phase: String, tick: int, resets: int = 0) -> Dictionary:
    return {"phase": phase, "tick": tick, "resets": resets,
        "window_focused": true, "window_drawable": true, "frames_drawn": maxi(0, tick)}

func window_context(focused: bool, drawable: bool, drawn: int) -> Dictionary:
    var value := context("combat", 0)
    value.merge({"window_focused": focused, "window_drawable": drawable, "frames_drawn": drawn}, true)
    return value

func check(value: bool, message: String) -> void:
    if not value:
        valid = false
        push_error(message)

func _initialize() -> void:
    var trace = Trace.new()
    var before := context("combat", 42)
    trace.enter(1000, before, {"draw": 2})
    trace.mark("native", 2000)
    trace.finish(3000, context("combat", 43))
    before.tick = 999 # Caller mutation must not alter retained evidence.
    trace.enter(1062000, context("combat", 43), {"draw": 3})
    trace.finish(1152000, context("combat", 44))
    var report: Dictionary = trace.report()
    check(report.interval_samples == 1 and report.callbacks_completed == 2, "Finalization invented a following interval")
    var row: Dictionary = report.maximum_context
    check(row.wall_ms == 1061.0 and row.body_ms == 2.0 and row.entry.tick == 42, "Slow interval was assigned to the wrong callback")
    check(row.outside_measured_callback_ms == 1059.0 and report.maximum_body_ms == 90.0, "Body/residual timing is incorrect")
    check(row.pipeline_delta.draw == 1.0, "Counter delta was lost")

    var transitions = Trace.new()
    var phases := ["intro", "combat", "stage_transition", "settling", "high_score", "intro", "menu", "capture", "network_wait"]
    for index in phases.size():
        transitions.enter(index * 10000, context(phases[index], index if index < 4 else 0, 1 if index >= 4 else 0), {})
        transitions.finish(index * 10000 + 1000, context(phases[index], index, 1 if index >= 3 else 0))
    var all_phases: Dictionary = transitions.report()
    check(all_phases.interval_samples == 8 and all_phases.phase_counts.size() == 7 and all_phases.phase_counts.intro == 2 and all_phases.phase_counts.has("menu") and all_phases.phase_counts.has("capture") and all_phases.phase_counts.has("stage_transition"), "Early/inactive/transition phases were excluded")
    check(all_phases.maximum_all_phases_ms == 10.0 and all_phases.slow_count == 0, "Warmup or fabricated slow rows contaminated all-phase metrics")

    var bounded = Trace.new()
    var clock := 0
    for index in 201:
        bounded.enter(clock, context("combat", index), {})
        bounded.finish(clock + 1000, context("combat", index + 1))
        clock += 40000 + index * 1000
    var result: Dictionary = bounded.report()
    check(result.slow_count == 200 and result.interval_samples == 200, "Bounded retention lost the full count")
    check(result.first_slow.size() == 16 and result.worst_slow.size() == 64, "Slow records exceeded their fixed budgets")
    check(result.first_slow[0].callback == 1 and result.worst_slow[0].callback == 200 and result.maximum_all_phases_ms == 239.0, "First/worst retention lost the actual maximum")
    var deltas: Dictionary = Trace.counter_delta({"draw": 8, "mesh": 3}, {"draw": 2})
    check(deltas.draw == null and deltas.mesh == null, "Reset/missing counters must be unavailable")

    var windows = Trace.new()
    var states := [[true, true, 10], [true, true, 11], [false, true, 12],
        [false, false, 12], [true, true, 13], [true, true, 0], [true, true, 1]]
    clock = 0
    for index in states.size():
        var state: Array = states[index]
        windows.enter(clock, window_context(state[0], state[1], state[2]), {})
        windows.finish(clock + 1000, window_context(false if index == 1 else state[0],
            false if index == 2 else state[1], state[2]))
        clock += (index + 1) * 10000
    var window_result: Dictionary = windows.report().window_render_diagnostics
    var groups: Dictionary = window_result.categories
    check(groups.draw_advanced_focused_drawable.samples == 2 and groups.draw_advanced_focused_drawable.maximum_ms == 60.0,
        "Focused/drawable intervals require actual draw counter advancement")
    check(groups.draw_advanced_other_window_state.samples == 2 and groups.no_draw_advance.samples == 1 and groups.counter_unavailable.samples == 1,
        "Occluded/reset/transition intervals were lost or counted as focused rendering")
    check(window_result.focused_interval_counts == {"all_true": 3, "all_false": 1, "changed": 2, "unavailable": 0}
        and window_result.frames_drawn_delta_total == 4, "Window coverage or actual draw count changed")
    check(window_result.observation_count == 14 and window_result.transition_count == 3 and window_result.transitions.size() == 3,
        "Sampled intra-callback window transitions were omitted")
    check(windows.report().maximum_context.window_interval.category == "draw_advanced_focused_drawable"
        and windows.report().maximum_context.next_window.frames_drawn == 1, "Retained slow rows lack following draw evidence")
    var missing := window_context(true, true, 2)
    missing.erase("frames_drawn")
    check(Trace.classify_window_interval(missing, missing, missing).category == "counter_unavailable",
        "Missing draw counter fabricated rendered work")
    var incomplete := window_context(true, true, 2)
    incomplete.erase("window_drawable")
    check(Trace.classify_window_interval(incomplete, incomplete, window_context(true, true, 3)).category == "draw_advanced_other_window_state",
        "Missing window state fabricated focused/drawable rendering")
    var toggles = Trace.new()
    for index in 101:
        toggles.enter(index * 10000, window_context(index % 2 == 0, true, index), {})
        toggles.finish(index * 10000 + 1000, window_context(index % 2 == 0, true, index))
    var toggle_report: Dictionary = toggles.report().window_render_diagnostics
    check(toggle_report.transition_count == 100 and toggle_report.transitions.size() == 32
        and toggle_report.transitions[31].observation == 65, "Window transitions exceeded their fixed budget or lost their total")
    if valid: print("TANKS_FRAME_TRACE_PASSED prior-callback/all-phases/bounds/ownership/counter-reset/finalization/window-draw-coverage/window-transition-bounds")
    quit(0 if valid else 1)
