#!/usr/bin/env python3
"""Compare the staged sample's Mobile and Forward+ standalone Metal rendering.

Runs one window at a time, with no editor, PNG capture or artificial fixed FPS.
The deterministic simulation input tape is independent of observed frame times.
Build and import first with make godot-sample. This is sample evidence, not the
repository's immutable candidate publication or long-duration release gate.
"""

import argparse
import datetime
import json
import math
from pathlib import Path
import platform
import re
import subprocess
import sys
import time


SCRIPT_DIRECTORY = Path(__file__).resolve().parent
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))
import test_godot_import as gate

ROOT = SCRIPT_DIRECTORY.parent
DEFAULT_LOGS = ROOT / "build/release-evidence/godot-sample-20260916/benchmarks"
RENDERERS = ("mobile", "forward_plus")
WARMUP_SAMPLES = 120


def source_manifest():
    paths = set()
    for extension in ("*.cpp", "*.h", "*.inl"):
        paths.update((ROOT / "src").rglob(extension))
    # Finder's view metadata is neither a source nor a packaged resource.
    paths.update(path for path in (ROOT / "godot").rglob("*")
                 if path.is_file() and path.name != ".DS_Store")
    paths.update(ROOT / "scripts" / name for name in (
        "benchmark_godot_sample.py", "test_godot_import.py", "prepare_godot_sample.py",
        "sample_macos_thermal.swift"))
    paths.add(ROOT / "build/godot/libtanks_sample.dylib")
    return {str(path.relative_to(ROOT)): gate.digest(path) for path in sorted(paths)}


def command_for(godot, project, engine_log, renderer, frames, stage, seed, seconds=0, stress=False,
                trace_slow_frames=False, pixel=False, wide_coop=False):
    command = [str(godot), "--path", str(project), "--rendering-method", renderer,
            "--rendering-driver", "metal", "--windowed", "--resolution", "1280x720",
            "--log-file", str(engine_log), "--", "--demo", "--benchmark",
            f"--stage={stage}", f"--seed={seed}", f"--frames={0 if seconds else frames}", "--render-size=1920x1080"]
    if seconds:
        command.append(f"--benchmark-seconds={seconds}")
    if stress:
        command.append("--benchmark-stress")
    if trace_slow_frames:
        command.append("--trace-slow-frames")
    if pixel:
        command.append("--pixel")
    if wide_coop:
        command.append("--benchmark-wide")
    return command


def validate_slow_trace(report):
    """Check coverage and retained prior-callback evidence, not a frame-budget pass."""
    trace = report.get("slow_frame_trace")
    if not isinstance(trace, dict) or trace.get("schema") not in ("tanks3d-slow-frames-v1", "tanks3d-slow-frames-v2"):
        raise RuntimeError("Requested slow-frame trace is missing or has an unknown schema")

    def require(condition, detail):
        if not condition:
            raise RuntimeError("Invalid slow-frame trace: " + detail)

    def number(value, positive=False):
        return type(value) in (int, float) and math.isfinite(value) and (value > 0 if positive else value >= 0)

    def integer(value, minimum=0):
        return type(value) is int and value >= minimum

    observed, completed, samples = (trace.get(key) for key in
                                     ("callbacks_observed", "callbacks_completed", "interval_samples"))
    require(integer(observed, 2) and completed == observed and type(completed) is int and
            integer(samples, 1) and samples == observed - 1 and observed == report["frames"],
            "all callbacks must finish and every entry-to-entry interval must be counted")
    require(trace.get("clock") == "Time.get_ticks_usec", "wrong monotonic clock")
    threshold = trace.get("threshold_ms")
    require(number(threshold, True) and math.isclose(threshold, 1000 / 30, abs_tol=1e-6),
            "unexpected slow-frame threshold")
    for key in ("initialization_ms", "maximum_body_ms", "maximum_all_phases_ms",
                "p95_all_phases_ms", "p99_all_phases_ms"):
        require(number(trace.get(key)), "non-finite or negative " + key)
    maximum = trace["maximum_all_phases_ms"]
    require(0 < trace["p95_all_phases_ms"] <= trace["p99_all_phases_ms"] <= maximum + 0.010001,
            "all-phase percentile bounds")
    phases = trace.get("phase_counts")
    require(isinstance(phases, dict) and bool(phases) and
            all(isinstance(key, str) and key and integer(value, 1) for key, value in phases.items()) and
            sum(phases.values()) == samples, "phase counts do not cover every interval")
    slow = trace.get("slow_count")
    require(integer(slow) and slow <= samples, "invalid slow interval count")
    retained = {}

    def context(value):
        require(isinstance(value, dict) and isinstance(value.get("phase"), str) and value["phase"] and
                integer(value.get("tick"), -1) and integer(value.get("resets")), "invalid phase/tick context")
        # Native ticks may reset. Counts describe net observations, not allocations or cache misses.
        for key in ("stage", "terrain_scans", "cached_meshes", "cached_materials",
                    "effects", "vehicles", "nodes", "resources"):
            require(integer(value.get(key), 1 if key == "stage" else 0), "invalid context count: " + key)

    def row(value):
        require(isinstance(value, dict), "missing retained callback")
        callback = value.get("callback")
        require(integer(callback, 1) and callback <= samples, "retained callback outside observed interval range")
        require(integer(value.get("begin_usec")), "invalid callback start time")
        for key in ("wall_ms", "body_ms", "outside_measured_callback_ms", "observed_seconds"):
            require(number(value.get(key), key == "wall_ms"), "invalid retained " + key)
        require(value["body_ms"] <= value["wall_ms"] + 0.001 and
                value["body_ms"] <= trace["maximum_body_ms"] + 0.001 and
                value["wall_ms"] <= maximum + 0.001, "prior body/wall maximum mismatch")
        require(math.isclose(value["outside_measured_callback_ms"],
                value["wall_ms"] - value["body_ms"], abs_tol=0.001), "incorrect outside-callback residual")
        require(value["observed_seconds"] <= report["wall_observed_seconds"] + 0.001,
                "retained observation exceeds run duration")
        context(value.get("entry"))
        context(value.get("exit"))
        require(isinstance(value.get("next_phase"), str) and bool(value["next_phase"]), "missing next phase")
        require(type(value.get("did_restart")) is bool and value["did_restart"] ==
                (value["exit"]["resets"] > value["entry"]["resets"]), "restart context mismatch")
        segments = value.get("segments_ms")
        require(isinstance(segments, dict) and bool(segments) and
                all(isinstance(key, str) and key and number(duration) for key, duration in segments.items()) and
                math.isclose(sum(segments.values()), value["body_ms"], abs_tol=0.001),
                "callback segments do not cover its measured body")
        before, after, delta = (value.get(key) for key in
                               ("pipeline_counters", "pipeline_counters_at_next_entry", "pipeline_delta"))
        require(all(isinstance(item, dict) for item in (before, after, delta)), "missing counter dictionaries")
        require(all(isinstance(key, str) and key and number(count)
                    for item in (before, after) for key, count in item.items()), "invalid observed counter")
        require(set(delta) == set(before), "counter delta keys mismatch")
        for key, count in before.items():
            expected = after[key] - count if key in after and after[key] >= count else None
            require(delta[key] is None if expected is None else
                    number(delta[key]) and math.isclose(delta[key], expected, abs_tol=1e-6),
                    "counter reset/missing value must remain unavailable")
        if callback in retained:
            require(value == retained[callback], "one callback has conflicting retained evidence")
        retained[callback] = value

    first, worst = trace.get("first_slow"), trace.get("worst_slow")
    require(isinstance(first, list) and isinstance(worst, list) and
            len(first) == min(slow, 16) and len(worst) == min(slow, 64), "first/worst retention bounds")
    for rows in (first, worst):
        for value in rows:
            row(value)
            require(value["wall_ms"] > threshold, "non-slow callback in slow retention")
        require(len({value["callback"] for value in rows}) == len(rows), "duplicate retained callbacks")
    require(first == sorted(first, key=lambda value: value["callback"]), "first callbacks out of order")
    require(worst == sorted(worst, key=lambda value: (-value["wall_ms"], value["callback"])),
            "worst callbacks out of order")
    largest = trace.get("maximum_context")
    row(largest)
    require(math.isclose(largest["wall_ms"], maximum, abs_tol=0.001), "maximum context does not match full-run maximum")
    require((slow > 0) == (maximum > threshold), "slow count disagrees with maximum")
    if slow:
        require(math.isclose(worst[0]["wall_ms"], maximum, abs_tol=0.001), "worst retention lost the maximum")
    require(len({value["callback"] for value in first + worst}) <= slow,
            "retained callbacks exceed total slow count")
    if trace["schema"] == "tanks3d-slow-frames-v2" or "window_render_diagnostics" in trace:
        validate_window_diagnostics(trace, retained, report)
    return trace


def validate_window_diagnostics(trace, retained, report):
    """Validate draw observations separately from callback throughput/present time."""
    def require(condition, detail):
        if not condition:
            raise RuntimeError("Invalid window/render diagnostics: " + detail)

    def integer(value, minimum=0):
        return type(value) is int and value >= minimum

    def number(value):
        return type(value) in (int, float) and math.isfinite(value) and value >= 0

    diagnostics = trace.get("window_render_diagnostics")
    require(isinstance(diagnostics, dict) and diagnostics.get("schema") == "tanks3d-window-render-v1",
            "missing coverage schema")
    require(diagnostics.get("counter") == "Engine.get_frames_drawn" and
            diagnostics.get("counter_semantics") == "engine_draw_calls_not_present", "draw counter is not a present clock")
    samples = trace["interval_samples"]
    require(type(diagnostics.get("interval_samples")) is int and diagnostics["interval_samples"] == samples,
            "interval coverage differs from all-phase trace")
    categories = diagnostics.get("categories")
    expected_groups = {"draw_advanced_focused_drawable", "draw_advanced_other_window_state",
                       "no_draw_advance", "counter_unavailable"}
    require(isinstance(categories, dict) and set(categories) == expected_groups, "missing interval categories")
    for name, group in categories.items():
        require(isinstance(group, dict) and integer(group.get("samples")) and integer(group.get("slow_count")) and
                group["slow_count"] <= group["samples"], "invalid category counts: " + name)
        require(all(number(group.get(key)) for key in ("p50_ms", "p95_ms", "p99_ms", "maximum_ms")),
                "non-finite category timing: " + name)
        if group["samples"]:
            require(0 < group["p50_ms"] <= group["p95_ms"] <= group["p99_ms"] <= group["maximum_ms"] + 0.010001,
                    "category percentile bounds: " + name)
            require((group["slow_count"] > 0) == (group["maximum_ms"] > trace["threshold_ms"]),
                    "category slow count disagrees with maximum: " + name)
        else:
            require(all(group[key] == 0 for key in ("slow_count", "p50_ms", "p95_ms", "p99_ms", "maximum_ms")),
                    "empty category fabricated timing: " + name)
    require(sum(group["samples"] for group in categories.values()) == samples and
            sum(group["slow_count"] for group in categories.values()) == trace["slow_count"],
            "categories do not partition all intervals and slow intervals")
    require(math.isclose(max(group["maximum_ms"] for group in categories.values()),
                         trace["maximum_all_phases_ms"], abs_tol=0.001), "category partition lost maximum")
    flag_groups = {"all_true", "all_false", "changed", "unavailable"}
    for key in ("focused_interval_counts", "drawable_interval_counts"):
        counts = diagnostics.get(key)
        require(isinstance(counts, dict) and set(counts) == flag_groups and
                all(integer(value) for value in counts.values()) and sum(counts.values()) == samples,
                "window flag coverage mismatch: " + key)
        require(counts["unavailable"] == 0, "window state was not actually observed: " + key)
    qualified = categories["draw_advanced_focused_drawable"]["samples"]
    require(qualified <= min(diagnostics["focused_interval_counts"]["all_true"],
                             diagnostics["drawable_interval_counts"]["all_true"]), "unqualified intervals counted as focused/drawable")
    advanced = qualified + categories["draw_advanced_other_window_state"]["samples"]
    total_delta = diagnostics.get("frames_drawn_delta_total")
    require(integer(total_delta) and total_delta >= advanced and (total_delta > 0) == (advanced > 0),
            "draw increments disagree with advancing interval count")
    observations = diagnostics.get("observation_count")
    require(type(observations) is int and observations == 2 * trace["callbacks_observed"] and
            type(diagnostics.get("missing_window_observations")) is int and diagnostics["missing_window_observations"] == 0,
            "every callback must observe window state at entry and exit")

    def window_state(value, counter=False):
        require(isinstance(value, dict) and all(type(value.get(key)) is bool
                for key in ("window_focused", "window_drawable")), "missing or non-boolean sampled window state")
        if counter:
            require(integer(value.get("frames_drawn")), "missing or invalid sampled draw counter")

    count, transitions = diagnostics.get("transition_count"), diagnostics.get("transitions")
    require(integer(count) and count < observations and isinstance(transitions, list) and
            len(transitions) == min(count, 32), "window transition retention bounds")
    previous_transition = None
    for transition in transitions:
        require(isinstance(transition, dict), "invalid transition")
        observation = transition.get("observation")
        require(integer(observation, 2) and observation <= observations and
                type(transition.get("callback")) is int and transition["callback"] == (observation + 1) // 2 and
                transition.get("boundary") == ("entry" if observation % 2 else "exit"), "invalid transition boundary")
        require(number(transition.get("observed_seconds")) and
                transition["observed_seconds"] <= report["wall_observed_seconds"] + 0.001,
                "invalid transition time")
        window_state(transition.get("before"))
        window_state(transition.get("after"))
        require(set(transition["before"]) == {"window_focused", "window_drawable"} and
                set(transition["after"]) == set(transition["before"]) and transition["before"] != transition["after"],
                "transition must change sampled window state")
        if previous_transition:
            require(observation > previous_transition["observation"] and
                    transition["observed_seconds"] >= previous_transition["observed_seconds"] and
                    transition["before"] == previous_transition["after"], "first transitions lost ordering or continuity")
        previous_transition = transition
    changed = max(diagnostics[key]["changed"] for key in ("focused_interval_counts", "drawable_interval_counts"))
    require(changed <= count, "sampled flag changes lack transition observations")
    if count == 0:
        require(all(not (diagnostics[key]["all_true"] and diagnostics[key]["all_false"])
                    for key in ("focused_interval_counts", "drawable_interval_counts")), "window states changed without transitions")

    retained_counts = {name: 0 for name in categories}
    retained_slow = dict(retained_counts)
    retained_delta = 0
    for row in retained.values():
        states = (row["entry"], row["exit"], row.get("next_window"))
        for state in states:
            window_state(state, counter=True)
        def flag(key):
            values = [state[key] for state in states]
            return "all_true" if all(values) else "all_false" if not any(values) else "changed"
        focused, drawable = flag("window_focused"), flag("window_drawable")
        before, ending, after = (state["frames_drawn"] for state in states)
        delta = after - before if before <= ending <= after else None
        category = ("counter_unavailable" if delta is None else "no_draw_advance" if delta == 0 else
                    "draw_advanced_focused_drawable" if focused == drawable == "all_true" else "draw_advanced_other_window_state")
        expected = {"category": category, "focused": focused, "drawable": drawable, "frames_drawn_delta": delta}
        actual = row.get("window_interval")
        require(isinstance(actual, dict) and actual == expected and
                (actual["frames_drawn_delta"] is None or integer(actual["frames_drawn_delta"])),
                "retained interval was classified without its actual draw/window evidence")
        require(row["wall_ms"] <= categories[category]["maximum_ms"] + 0.001,
                "retained row exceeds its category maximum")
        retained_counts[category] += 1
        retained_slow[category] += row["wall_ms"] > trace["threshold_ms"]
        retained_delta += delta or 0
        require(diagnostics["focused_interval_counts"][focused] > 0 and diagnostics["drawable_interval_counts"][drawable] > 0,
                "retained window state missing from coverage totals")
    require(all(retained_counts[name] <= categories[name]["samples"] and
                retained_slow[name] <= categories[name]["slow_count"] for name in categories) and retained_delta <= total_delta,
            "retained draw evidence exceeds full-run totals")
    return diagnostics


def draw_observation_summary(report):
    diagnostics = report.get("slow_frame_trace", {}).get("window_render_diagnostics")
    if not diagnostics:
        return {"available": False, "limitation": "Window/draw coverage was not recorded; callback throughput is not rendered-frame performance"}
    qualified = diagnostics["categories"]["draw_advanced_focused_drawable"]
    return {"available": True, "focused_drawable_draw_advancing_intervals": qualified,
            "excluded_intervals": diagnostics["interval_samples"] - qualified["samples"],
            "draw_counter_increment_total": diagnostics["frames_drawn_delta_total"],
            "limitation": "All phases, no warmup exclusion. Sampled window state and engine draw increments; not GPU completion or display-present timestamps"}


def validate_report(console, frames, renderer, stage, seed, seconds=0, stress=False,
                    trace_slow_frames=False, pixel=False, wide_coop=False):
    if seconds:
        matches = re.findall(r"^TANKS_SAMPLE_REPORT (.+)$", console, re.MULTILINE)
        if len(matches) != 1:
            raise RuntimeError("Duration run did not produce exactly one report")
        frames = json.loads(matches[0]).get("frames")
        if not isinstance(frames, int) or frames <= WARMUP_SAMPLES:
            raise RuntimeError("Duration run did not complete enough frames")
    report = gate.validate_report(console, frames)
    if report.get("renderer") != renderer or report.get("driver") != "metal":
        raise RuntimeError("Benchmark renderer/driver differs from the requested native Metal configuration")
    if report.get("stage") != stage or report.get("seed") != seed:
        raise RuntimeError("Benchmark map/seed differs from the requested input tape")
    if report.get("render_size") not in ("(1920, 1080)", [1920, 1080]):
        raise RuntimeError("Benchmark did not render internally at 1920x1080")
    if report.get("pixel_style") is not pixel:
        raise RuntimeError("Benchmark Pixel Style differs from the requested mode")
    if report.get("max_draw_calls", 0) <= 0 or report.get("max_objects", 0) <= 0:
        raise RuntimeError("Benchmark did not report rendered geometry (headless execution is insufficient)")
    if report.get("active_gameplay_frames", 0) <= WARMUP_SAMPLES or report.get("timing_samples", 0) <= 0:
        raise RuntimeError("Benchmark did not report active gameplay measurements after warmup")
    # These are measured process-frame/update times, not configured engine rates.
    for key in ("p50_frame_ms", "p95_frame_ms", "p99_frame_ms", "p95_update_ms"):
        value = report.get(key)
        if not isinstance(value, (float, int)) or not math.isfinite(value) or value <= 0:
            raise RuntimeError(f"Benchmark has no valid steady-state measurement for {key}")
    # Godot _process(delta) is paced/smoothed and can differ from actual elapsed
    # time. Require a separate monotonic clock measurement for new comparisons.
    if report.get("wall_sample_clock") != "Time.get_ticks_usec" or report.get("wall_timing_samples", 0) <= 0:
        raise RuntimeError("Benchmark lacks independently measured monotonic inter-frame samples")
    for key in ("p50_wall_frame_ms", "p95_wall_frame_ms", "p99_wall_frame_ms", "wall_observed_seconds"):
        value = report.get(key)
        if not isinstance(value, (float, int)) or not math.isfinite(value) or value <= 0:
            raise RuntimeError(f"Benchmark has no valid monotonic measurement for {key}")
    if not re.search(r"^Metal\s+\S+.*Using Device #\d+:", console, flags=re.MULTILINE):
        raise RuntimeError("Native Metal renderer startup was not observed in the standalone log")
    if seconds:
        if report.get("benchmark_seconds") != seconds or report["wall_observed_seconds"] < seconds:
            raise RuntimeError("Sustained run stopped before its requested wall duration")
        windows = report.get("timing_windows", [])
        if (not windows or windows[-1].get("end_seconds", 0) < seconds
                or sum(window.get("samples", 0) for window in windows) != report["wall_timing_samples"]):
            raise RuntimeError("Sustained run lost per-minute timing samples")
    if stress:
        settings = report.get("benchmark_settings", {})
        expected = {"players": 2, "ai_p2": True, "lives": 99, "max_hp": 6,
                    "enemy_speed": 30, "enemy_fire": 30, "enemy_spawn": 30}
        if (report.get("benchmark_stress") is not True or
                any(settings.get(key) != value for key, value in expected.items()) or
                report.get("max_live_enemies", 0) < 4 or report.get("max_shells", 0) < 4):
            raise RuntimeError("Stress run did not exercise the requested rules and full enemy occupancy")
    if trace_slow_frames:
        validate_slow_trace(report)
        coverage = draw_observation_summary(report)
        if coverage["available"] and not coverage["focused_drawable_draw_advancing_intervals"]["samples"]:
            raise RuntimeError("Benchmark recorded no focused/drawable interval with an advancing draw counter")
    if wide_coop:
        settings = report.get("benchmark_settings", {})
        count = report.get("wide_gameplay_frames")
        separation, span = report.get("max_player_separation"), report.get("max_camera_span")
        coverage = report.get("wide_wall_seconds")
        drawn_coverage = report.get("wide_draw_wall_seconds")
        timings = report.get("wide_draw_timing", {})
        if (report.get("benchmark_wide") is not True or settings.get("players") != 2 or
                settings.get("ai_p2") is not False or type(count) is not int or
                not max(WARMUP_SAMPLES, report["active_gameplay_frames"] * .25) < count <= report["active_gameplay_frames"] or
                any(type(value) not in (int, float) or not math.isfinite(value) for value in (separation, span, coverage, drawn_coverage)) or
                not report["wall_observed_seconds"] * .25 <= coverage <= report["wall_observed_seconds"] or
                not report["wall_observed_seconds"] * .25 <= drawn_coverage <= coverage or
                type(timings.get("wall_timing_samples")) is not int or
                not 0 < timings["wall_timing_samples"] <= count - WARMUP_SAMPLES or
                any(type(timings.get(key)) not in (int, float) or not math.isfinite(timings[key]) or timings[key] <= 0
                    for key in ("p50_wall_frame_ms", "p95_wall_frame_ms", "p99_wall_frame_ms")) or
                separation < 10.0 or span <= 18.51):
            raise RuntimeError("Wide co-op run lacks actual separated players and expanded-camera combat samples")
    return report


def thermal_sampler():
    """Compile a small read-only Foundation observer, without privileged tools."""
    folder = ROOT / "build/godot/thermal"
    folder.mkdir(parents=True, exist_ok=True)
    executable = folder / "sample-thermal"
    subprocess.run(["xcrun", "swiftc", "-O", "-module-cache-path", str(folder / "module-cache"),
                    str(ROOT / "scripts/sample_macos_thermal.swift"), "-o", str(executable)],
                   cwd=ROOT, check=True, capture_output=True, timeout=60)
    return executable


def sample_rss(pid):
    # Select precisely the child PID; never aggregate editor, other game or
    # script processes. macOS ps reports resident set size in KiB.
    result = subprocess.run(["ps", "-p", str(pid), "-o", "pid=,rss="],
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
    if result.returncode == 1 and not result.stdout.strip():
        return None  # Process exited between poll and ps.
    if result.returncode != 0:
        raise RuntimeError(f"Cannot sample benchmark child RSS: {result.stderr.strip()}")
    fields = result.stdout.split()
    if len(fields) != 2 or int(fields[0]) != pid or int(fields[1]) < 0:
        raise RuntimeError(f"Unexpected RSS sample for benchmark child PID {pid}")
    return int(fields[1]) * 1024


def terminate_child(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)


def run_renderer(args, renderer, sources, staged):
    output_dir = args.log_dir / renderer
    output_dir.mkdir(parents=True, exist_ok=True)
    receipt_file = output_dir / "benchmark.json"
    if receipt_file.exists():
        raise RuntimeError(f"Existing benchmark receipt would be overwritten: {receipt_file}; choose another --log-dir")
    console_log = output_dir / "console.log"
    engine_log = output_dir / "engine.log"
    engine_log.unlink(missing_ok=True)
    command = command_for(args.godot, args.project, engine_log, renderer, args.frames, args.stage, args.seed,
                          args.seconds, args.stress, args.trace_slow_frames, args.pixel, args.wide_coop)
    started = datetime.datetime.now(datetime.timezone.utc).isoformat()
    start = time.monotonic()
    rss_samples = []
    process = None
    thermal_process = None
    failure = None
    next_progress = 60.0
    workload = f"{args.seconds:g} wall seconds" if args.seconds else f"{args.frames} frames"
    print(f"Starting {renderer}: {workload}, stage {args.stage}, stress={args.stress}, 1920x1080, native Metal", flush=True)
    try:
        thermal_log = output_dir / "thermal.jsonl"
        with console_log.open("w") as output, thermal_log.open("w") as thermal_output:
            if args.thermal_binary:
                thermal_process = subprocess.Popen([str(args.thermal_binary)], cwd=ROOT,
                                                   stdout=thermal_output, stderr=subprocess.STDOUT)
            process = subprocess.Popen(command, cwd=ROOT, text=True, stdout=output, stderr=subprocess.STDOUT)
            while process.poll() is None:
                age = time.monotonic() - start
                if age > args.timeout:
                    raise RuntimeError(f"{renderer} timed out after {args.timeout}s")
                rss = sample_rss(process.pid)
                if rss is not None:
                    rss_samples.append({"elapsed_seconds": round(age, 3), "rss_bytes": rss})
                if age >= next_progress:
                    memory = f", RSS {rss / 1048576:.1f} MiB" if rss is not None else ""
                    print(f"Running {renderer}: {age:.0f}s elapsed{memory}", flush=True)
                    next_progress += 60.0
                try:
                    process.wait(timeout=1)
                except subprocess.TimeoutExpired:
                    pass
            elapsed = time.monotonic() - start
            if thermal_process is not None:
                if thermal_process.poll() is not None:
                    raise RuntimeError("Thermal observer exited before the rendered test")
                terminate_child(thermal_process)
        console = console_log.read_text(errors="replace")
        engine_text = engine_log.read_text(errors="replace") if engine_log.exists() else ""
        gate.validate_output(console + "\n" + engine_text, process.returncode, renderer)
        report = validate_report(gate.ANSI.sub("", console), args.frames, renderer, args.stage, args.seed,
                                 args.seconds, args.stress, args.trace_slow_frames, args.pixel, args.wide_coop)
        if report["wall_observed_seconds"] > elapsed + 1.0:
            raise RuntimeError("Reported monotonic observation exceeds the independent child-process elapsed time")
        if not rss_samples:
            raise RuntimeError("No resident-memory sample was captured for the actual game process")
        if source_manifest() != sources or gate.validate_staged(args.project) != staged:
            raise RuntimeError("Sample sources or staged resources changed while the benchmark was running")
        thermal_samples = [json.loads(line) for line in thermal_log.read_text().splitlines()] if args.thermal_binary else []
        if args.thermal_binary and (not thermal_samples or
                thermal_samples[-1]["elapsed_seconds"] < max(0, report["wall_observed_seconds"] - 10)):
            raise RuntimeError("Thermal observations do not cover the rendered test")
        receipt = {
            "schema": "tanks3d-godot-sample-benchmark-v3", "status": "passed",
            "started_utc": started, "argv": command, "child_pid": process.pid,
            "elapsed_seconds": elapsed, "exit_code": process.returncode,
            "overall_wall_ms_per_simulation_step": report["wall_observed_seconds"] * 1000.0 / report["frames"],
            "source_sha256": sources, "staged_sha256": staged,
            "standalone": True, "native_driver": "metal", "headless": False,
            "requested_window_pixels": [1280, 720], "internal_render_pixels": [1920, 1080],
            "warmup_active_samples_excluded_from_frame_percentiles": WARMUP_SAMPLES,
            "frame_percentiles": report,
            "primary_frame_clock": "Time.get_ticks_usec; monotonic intervals between process callbacks",
            "peak_sampled_rss_bytes": max(sample["rss_bytes"] for sample in rss_samples),
            "rss_sample_period_seconds": 1, "rss_samples": rss_samples,
            "requested_wall_seconds": args.seconds, "stress": args.stress,
            "pixel_style": args.pixel, "wide_coop": args.wide_coop,
            "trace_slow_frames": args.trace_slow_frames,
            "draw_observations": draw_observation_summary(report),
            "thermal_observer": "Foundation ProcessInfo.thermalState" if args.thermal_binary else None,
            "thermal_samples": thermal_samples,
            "host": {"system": platform.system(), "macos": platform.mac_ver()[0], "machine": platform.machine()},
            "limitations": [
                "Engine delta is retained separately from actual monotonic inter-frame wall time",
                "Wall intervals include vsync, pacing and CPU work; they are not GPU-only time or display-present timestamps",
                "Unfocused or occluded callbacks remain in process metrics; only trace draw-observation categories separate them",
                "Resident memory is sampled about once per second; transient peaks may be missed",
                "Passing validates the measurement workload, not a release frame-budget attestation",
                "Thermal classes are OS pressure observations, not temperatures or power measurements",
                "No screenshots or PNG capture occurred during measurement",
            ],
        }
        receipt_file.write_text(json.dumps(receipt, indent=2) + "\n")
        print(f"PASS {renderer}: process-interval p95 {report['p95_wall_frame_ms']:.2f} ms "
              f"(engine delta {report['p95_frame_ms']:.2f} ms), "
              f"sampled peak RSS {receipt['peak_sampled_rss_bytes'] / 1048576:.1f} MiB", flush=True)
        coverage = receipt["draw_observations"]
        if coverage["available"]:
            qualified = coverage["focused_drawable_draw_advancing_intervals"]
            print(f"Draw observations: {qualified['samples']} focused/drawable advancing intervals, "
                  f"p95 {qualified['p95_ms']:.2f} ms across all phases; {coverage['excluded_intervals']} excluded. "
                  "This is not display-present timing.", flush=True)
        return receipt
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        failure = str(error)
        raise
    finally:
        if process is not None:
            terminate_child(process)
        if thermal_process is not None:
            terminate_child(thermal_process)
        if failure:
            (output_dir / "failure.json").write_text(json.dumps({
                "status": "failed", "reason": failure, "argv": command,
                "started_utc": started, "source_sha256": sources, "rss_samples": rss_samples,
            }, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", type=Path, default=ROOT / "build/godot-tools/Godot.app/Contents/MacOS/Godot")
    parser.add_argument("--project", type=Path, default=ROOT / "build/godot/project")
    parser.add_argument("--log-dir", type=Path, default=DEFAULT_LOGS)
    parser.add_argument("--frames", type=int, default=3600)
    parser.add_argument("--stage", type=int, default=10)
    parser.add_argument("--seed", type=int, default=20260916)
    parser.add_argument("--timeout", type=float)
    parser.add_argument("--seconds", type=float, default=0, help="Use real wall duration instead of a fixed frame count (max7200)")
    parser.add_argument("--stress", action="store_true", help="Use legal +30%% enemy tuning, fixed P1 fire, AI P2, six HP and99lives")
    parser.add_argument("--pixel", action="store_true", help="Exercise and verify Pixel Style ON; default verifies OFF")
    parser.add_argument("--wide-coop", action="store_true", help="Move P1 north and hold P2 using native commands; verify actual expanded-camera coverage")
    parser.add_argument("--thermal", action="store_true", help="Record macOS thermal pressure without privileged access")
    parser.add_argument("--trace-slow-frames", action="store_true",
                        help="Record bounded all-phase slow intervals with prior-callback context")
    parser.add_argument("--renderer", choices=(*RENDERERS, "both"), default="both")
    args = parser.parse_args()
    args.timeout = args.timeout if args.timeout is not None else max(180, args.seconds + 120)
    args.project, args.log_dir, args.godot = args.project.resolve(), args.log_dir.resolve(), args.godot.resolve()
    if not args.project.is_relative_to(ROOT / "build") or not args.log_dir.is_relative_to(ROOT / "build"):
        parser.error("Project and logs must remain under the repository's build/ directory")
    if (args.frames <= WARMUP_SAMPLES or not 1 <= args.stage <= 35 or not math.isfinite(args.timeout)
            or args.timeout <= 0 or not math.isfinite(args.seconds) or not 0 <= args.seconds <= 7200):
        parser.error("Require more than 120 frames, a stage from 1 through 35 and a positive timeout")
    if args.seconds and (args.renderer == "both" or args.timeout <= args.seconds):
        parser.error("Duration tests require one renderer and a timeout longer than the requested duration")
    if args.thermal and platform.system() != "Darwin":
        parser.error("Thermal observation requires macOS")
    if args.wide_coop and args.stress:
        parser.error("Wide co-op uses two commanded players; stress uses AI P2. Run them separately.")
    args.log_dir.mkdir(parents=True, exist_ok=True)
    args.thermal_binary = thermal_sampler() if args.thermal else None
    sources = source_manifest()
    staged = gate.validate_staged(args.project)
    renderers = RENDERERS if args.renderer == "both" else (args.renderer,)
    receipts = [run_renderer(args, renderer, sources, staged) for renderer in renderers]
    if len(receipts) == 2:
        digests = {value["frame_percentiles"]["final_digest"] for value in receipts}
        if len(digests) != 1:
            raise RuntimeError("Renderer runs ended with different native simulation digests; comparison is invalid")
    summary = {"status": "passed", "runs": {renderer: {
        "receipt": str((args.log_dir / renderer / "benchmark.json").relative_to(ROOT)),
        "frame_percentiles": receipt["frame_percentiles"],
        "peak_sampled_rss_bytes": receipt["peak_sampled_rss_bytes"],
    } for renderer, receipt in zip(renderers, receipts)}}
    (args.log_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Benchmark complete: {(args.log_dir / 'summary.json').relative_to(ROOT)}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error)) from error
