"""Reject measurements that did not exercise the requested live renderer."""

import importlib.util
import copy
import json
from pathlib import Path
import unittest
from unittest.mock import patch
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("godot_benchmark", ROOT / "scripts/benchmark_godot_sample.py")
BENCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BENCH)


class GodotBenchmarkTests(unittest.TestCase):
    def test_manifest_ignores_finder_metadata_but_keeps_runtime_scripts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "godot").mkdir()
            (root / "godot/.DS_Store").write_bytes(b"Finder view state")
            (root / "godot/main.gd").write_text("extends Node\n")
            with patch.object(BENCH, "ROOT", root), patch.object(BENCH.gate, "digest", return_value="test-digest"):
                manifest = BENCH.source_manifest()
            self.assertNotIn("godot/.DS_Store", manifest)
            self.assertIn("godot/main.gd", manifest)

    def trace_row(self, callback=12, wall=1061.0):
        context = {"phase": "combat", "tick": 42, "resets": 0, "stage": 10,
                   "terrain_scans": 8, "cached_meshes": 40, "cached_materials": 5,
                   "effects": 3, "vehicles": 6, "nodes": 400, "resources": 100}
        counters = {"canvas": 1, "mesh": 4, "surface": 2, "draw": 0, "specialization": 4}
        return {"callback": callback, "begin_usec": 1000, "entry": context,
                "exit": context | {"tick": 43}, "next_phase": "combat", "did_restart": False,
                "wall_ms": wall, "body_ms": 2.0, "outside_measured_callback_ms": wall - 2.0,
                "observed_seconds": 2.0, "segments_ms": {"native": 1.0, "tail": 1.0},
                "pipeline_counters": counters, "pipeline_counters_at_next_entry": counters | {"draw": 1},
                "pipeline_delta": {"canvas": 0, "mesh": 0, "surface": 0, "draw": 1, "specialization": 0}}

    def trace(self, slow=True):
        largest = self.trace_row(wall=1061.0 if slow else 16.0)
        rows = [largest, self.trace_row(2145, 43.0)] if slow else []
        return {"schema": "tanks3d-slow-frames-v1", "clock": "Time.get_ticks_usec",
                "threshold_ms": 1000 / 30, "callbacks_observed": 3600, "callbacks_completed": 3600,
                "interval_samples": 3599, "initialization_ms": 140.0, "maximum_body_ms": 90.0,
                "p95_all_phases_ms": 16.0, "p99_all_phases_ms": 16.0,
                "maximum_all_phases_ms": largest["wall_ms"], "phase_counts": {"intro": 120, "combat": 3479},
                "slow_count": len(rows), "first_slow": copy.deepcopy(rows), "worst_slow": copy.deepcopy(rows),
                "maximum_context": copy.deepcopy(largest)}

    def validate_trace(self, trace):
        return BENCH.validate_report(self.console(slow_frame_trace=trace), 3600,
                                     "mobile", 10, 20260916, trace_slow_frames=True)

    def window_trace(self, slow=True):
        trace = self.trace(slow)
        trace["schema"] = "tanks3d-slow-frames-v2"
        for row in trace["first_slow"] + trace["worst_slow"] + [trace["maximum_context"]]:
            drawn = row["callback"]
            row["entry"].update(window_focused=True, window_drawable=True, frames_drawn=drawn)
            row["exit"].update(window_focused=True, window_drawable=True, frames_drawn=drawn)
            row["next_window"] = {"window_focused": True, "window_drawable": True, "frames_drawn": drawn + 1}
            row["window_interval"] = {"category": "draw_advanced_focused_drawable", "focused": "all_true",
                                      "drawable": "all_true", "frames_drawn_delta": 1}
        empty = {"samples": 0, "slow_count": 0, "p50_ms": 0, "p95_ms": 0, "p99_ms": 0, "maximum_ms": 0}
        categories = {key: dict(empty) for key in ("draw_advanced_focused_drawable", "draw_advanced_other_window_state",
                                                  "no_draw_advance", "counter_unavailable")}
        categories["draw_advanced_focused_drawable"] = {
            "samples": 3599, "slow_count": trace["slow_count"], "p50_ms": 16, "p95_ms": 16, "p99_ms": 16,
            "maximum_ms": trace["maximum_all_phases_ms"]}
        trace["window_render_diagnostics"] = {
            "schema": "tanks3d-window-render-v1", "counter": "Engine.get_frames_drawn",
            "counter_semantics": "engine_draw_calls_not_present", "interval_samples": 3599,
            "categories": categories, "frames_drawn_delta_total": 3599,
            "focused_interval_counts": {"all_true": 3599, "all_false": 0, "changed": 0, "unavailable": 0},
            "drawable_interval_counts": {"all_true": 3599, "all_false": 0, "changed": 0, "unavailable": 0},
            "observation_count": 7200, "missing_window_observations": 0, "transition_count": 0, "transitions": []}
        return trace

    def console(self, **updates):
        report = {"frames": 3600, "demo_fixed_step": True, "final_digest": "1234",
                  "renderer": "mobile", "driver": "metal", "stage": 10, "seed": 20260916,
                  "render_size": "(1920, 1080)", "pixel_style": False, "max_draw_calls": 80, "max_objects": 150,
                  "active_gameplay_frames": 3400, "timing_samples": 3280,
                  "p50_frame_ms": 16.67, "p95_frame_ms": 17.1, "p99_frame_ms": 18.4,
                  "p95_update_ms": 2.1, "p50_wall_frame_ms": 16.72,
                  "p95_wall_frame_ms": 17.4, "p99_wall_frame_ms": 19.1,
                  "wall_timing_samples": 3279, "wall_observed_seconds": 60.9,
                  "wall_sample_clock": "Time.get_ticks_usec"}
        report.update(updates)
        return ("Metal 4.0 - Forward Mobile - Using Device #0: Apple - Apple M2 (Apple8)\n"
                "TANKS_SAMPLE_READY renderer=mobile driver=metal\n"
                "TANKS_SAMPLE_REPORT " + json.dumps(report))

    def test_command_does_not_enable_fixed_fps_headless_or_capture(self):
        command = BENCH.command_for(Path("Godot"), Path("project"), Path("engine.log"), "mobile", 3600, 10, 20260916)
        self.assertNotIn("--fixed-fps", command)
        self.assertNotIn("--headless", command)
        self.assertNotIn("--editor", command)
        self.assertFalse(any(arg.startswith("--capture") for arg in command))
        self.assertIn("--render-size=1920x1080", command)

    def test_live_metal_report_passes(self):
        report = BENCH.validate_report(self.console(), 3600, "mobile", 10, 20260916)
        self.assertEqual(report["timing_samples"], 3280)

    def test_pixel_mode_must_be_observed_not_just_requested(self):
        for requested, reported in ((True, False), (False, True), (False, None), (True, 1)):
            with self.subTest(requested=requested, reported=reported), self.assertRaisesRegex(RuntimeError, "Pixel Style"):
                BENCH.validate_report(self.console(pixel_style=reported), 3600, "mobile", 10, 20260916, pixel=requested)
        BENCH.validate_report(self.console(pixel_style=True), 3600, "mobile", 10, 20260916, pixel=True)

    def test_wide_coop_requires_real_camera_and_separation_coverage(self):
        values = {"benchmark_wide": True, "benchmark_settings": {"players": 2, "ai_p2": False},
                  "wide_gameplay_frames": 900, "max_player_separation": 18.0, "max_camera_span": 28.0,
                  "wide_wall_seconds": 20.0, "wide_draw_wall_seconds": 18.0,
                  "wide_draw_timing": {"wall_timing_samples": 600, "p50_wall_frame_ms": 16.0,
                                       "p95_wall_frame_ms": 17.0, "p99_wall_frame_ms": 18.0}}
        BENCH.validate_report(self.console(**values), 3600, "mobile", 10, 20260916, wide_coop=True)
        for invalid in ({"wide_gameplay_frames": 0}, {"wide_gameplay_frames": 9999},
                        {"wide_gameplay_frames": 121}, {"wide_wall_seconds": 1.0},
                        {"wide_draw_wall_seconds": 0.0}, {"wide_draw_timing": {}},
                        {"max_camera_span": 18.5}, {"max_player_separation": 8.0},
                        {"max_camera_span": float("nan")}, {"benchmark_settings": {"players": 2, "ai_p2": True}}):
            with self.subTest(invalid=invalid), self.assertRaisesRegex(RuntimeError, "Wide co-op"):
                BENCH.validate_report(self.console(**(values | invalid)), 3600, "mobile", 10, 20260916, wide_coop=True)

    def test_visual_coverage_flags_reach_engine(self):
        command = BENCH.command_for(Path("Godot"), Path("project"), Path("engine.log"),
                                    "mobile", 3600, 1, 20260916, pixel=True, wide_coop=True)
        self.assertIn("--pixel", command)
        self.assertIn("--benchmark-wide", command)

    def test_headless_configuration_label_does_not_prove_metal(self):
        console = self.console().split("\n", 1)[1]
        with self.assertRaises(RuntimeError):
            BENCH.validate_report(console, 3600, "mobile", 10, 20260916)

    def test_no_geometry_fails(self):
        with self.assertRaises(RuntimeError):
            BENCH.validate_report(self.console(max_draw_calls=0), 3600, "mobile", 10, 20260916)

    def test_nan_measurement_fails(self):
        with self.assertRaises(RuntimeError):
            BENCH.validate_report(self.console(p95_frame_ms=float("nan")), 3600, "mobile", 10, 20260916)

    def test_no_active_samples_after_warmup_fails(self):
        with self.assertRaises(RuntimeError):
            BENCH.validate_report(self.console(timing_samples=0), 3600, "mobile", 10, 20260916)

    def test_wrong_renderer_fails(self):
        with self.assertRaises(RuntimeError):
            BENCH.validate_report(self.console(renderer="forward_plus"), 3600, "mobile", 10, 20260916)

    def test_smoothed_engine_delta_alone_does_not_pass(self):
        with self.assertRaises(RuntimeError):
            BENCH.validate_report(self.console(wall_sample_clock=None), 3600, "mobile", 10, 20260916)

    def test_nonfinite_monotonic_time_fails(self):
        with self.assertRaises(RuntimeError):
            BENCH.validate_report(self.console(p95_wall_frame_ms=float("inf")), 3600, "mobile", 10, 20260916)

    def test_duration_command_cannot_stop_at_default_frame_limit(self):
        command = BENCH.command_for(Path("Godot"), Path("project"), Path("engine.log"),
                                    "mobile", 3600, 35, 20260916, 1200, True)
        self.assertIn("--frames=0", command)
        self.assertIn("--benchmark-seconds=1200", command)
        self.assertIn("--benchmark-stress", command)

    def test_duration_requires_actual_wall_coverage_and_all_window_samples(self):
        values = {"frames": 140000, "benchmark_seconds": 1200, "wall_observed_seconds": 1200.01,
                  "timing_windows": [{"end_seconds": 1200.01, "samples": 3279}]}
        BENCH.validate_report(self.console(**values), 3600, "mobile", 10, 20260916, 1200)
        for invalid in ({"wall_observed_seconds": 60}, {"timing_windows": []},
                        {"timing_windows": [{"end_seconds": 1200.01, "samples": 1}]}):
            with self.assertRaises(RuntimeError):
                BENCH.validate_report(self.console(**(values | invalid)), 3600, "mobile", 10, 20260916, 1200)

    def test_stress_configuration_without_actual_combat_fails(self):
        values = {"benchmark_stress": True, "benchmark_settings": {"players": 2, "ai_p2": True,
                  "lives": 99, "max_hp": 6, "enemy_speed": 30, "enemy_fire": 30, "enemy_spawn": 30},
                  "max_live_enemies": 4, "max_shells": 8}
        BENCH.validate_report(self.console(**values), 3600, "mobile", 10, 20260916, stress=True)
        with self.assertRaises(RuntimeError):
            BENCH.validate_report(self.console(**(values | {"max_live_enemies": 1})),
                                  3600, "mobile", 10, 20260916, stress=True)

    def test_trace_is_opt_in_and_forwarded_to_engine(self):
        args = (Path("Godot"), Path("project"), Path("engine.log"), "mobile", 3600, 10, 20260916)
        self.assertNotIn("--trace-slow-frames", BENCH.command_for(*args))
        self.assertIn("--trace-slow-frames", BENCH.command_for(*args, trace_slow_frames=True))
        with self.assertRaisesRegex(RuntimeError, "missing"):
            BENCH.validate_report(self.console(), 3600, "mobile", 10, 20260916, trace_slow_frames=True)

    def test_prior_callback_trace_and_no_slow_frames_are_valid(self):
        self.validate_trace(self.trace())
        self.validate_trace(self.trace(False))

    def test_trace_rejects_partial_callbacks_or_dropped_inactive_phases(self):
        for change in ({"callbacks_completed": 3599}, {"interval_samples": 3400},
                       {"phase_counts": {"combat": 3479}}, {"callbacks_observed": 3601}):
            with self.subTest(change=change), self.assertRaises(RuntimeError):
                self.validate_trace(self.trace() | change)

    def test_trace_rejects_invalid_times_and_false_prior_body_attribution(self):
        for key, value in (("body_ms", 1062), ("outside_measured_callback_ms", 1000),
                           ("wall_ms", float("nan")), ("observed_seconds", 1000)):
            trace = self.trace()
            trace["maximum_context"][key] = value
            with self.subTest(key=key), self.assertRaises(RuntimeError):
                self.validate_trace(trace)
        with self.assertRaises(RuntimeError):
            self.validate_trace(self.trace() | {"p95_all_phases_ms": float("inf")})

    def test_trace_rejects_invalid_retention_or_lost_maximum(self):
        for change in ({"slow_count": 1}, {"first_slow": []}, {"worst_slow": []},
                       {"worst_slow": list(reversed(self.trace()["worst_slow"]))},
                       {"maximum_all_phases_ms": 2000}, {"slow_count": 4000}):
            with self.subTest(change=change), self.assertRaises(RuntimeError):
                self.validate_trace(self.trace() | change)

    def test_trace_rejects_conflicting_rows_bad_segments_and_duplicate_ids(self):
        trace = self.trace()
        trace["first_slow"][0]["entry"]["tick"] = 999
        with self.assertRaisesRegex(RuntimeError, "conflicting"):
            self.validate_trace(trace)
        trace = self.trace()
        trace["first_slow"][0]["segments_ms"]["native"] = 8
        with self.assertRaisesRegex(RuntimeError, "segments"):
            self.validate_trace(trace)
        trace = self.trace()
        trace["first_slow"][1] = copy.deepcopy(trace["first_slow"][0])
        with self.assertRaisesRegex(RuntimeError, "duplicate"):
            self.validate_trace(trace)

    def test_trace_counter_reset_is_unavailable_and_native_tick_may_reset(self):
        trace = self.trace(False)
        row = trace["maximum_context"]
        row["exit"]["tick"] = 0
        row["exit"]["resets"] = 1
        row["did_restart"] = True
        row["pipeline_counters_at_next_entry"]["mesh"] = 1
        del row["pipeline_counters_at_next_entry"]["surface"]
        row["pipeline_delta"]["mesh"] = None
        row["pipeline_delta"]["surface"] = None
        self.validate_trace(trace)
        row["pipeline_delta"]["mesh"] = 0
        with self.assertRaisesRegex(RuntimeError, "unavailable"):
            self.validate_trace(trace)

    def test_trace_retains_only_fixed_first_and_worst_budgets(self):
        trace = self.trace()
        rows = [self.trace_row(index + 1, 40 + index) for index in range(200)]
        trace.update(slow_count=200, first_slow=rows[:16], worst_slow=list(reversed(rows[-64:])),
                     maximum_context=rows[-1], maximum_all_phases_ms=239)
        self.validate_trace(trace)
        trace["worst_slow"].append(rows[0])
        with self.assertRaisesRegex(RuntimeError, "bounds"):
            self.validate_trace(trace)

    def test_window_trace_is_required_in_v2_but_old_v1_remains_valid(self):
        for slow in (False, True):
            self.validate_trace(self.trace(slow))
            self.validate_trace(self.window_trace(slow))
        trace = self.window_trace()
        del trace["window_render_diagnostics"]
        with self.assertRaisesRegex(RuntimeError, "coverage schema"):
            self.validate_trace(trace)

    def test_window_trace_rejects_unobserved_flags_and_incomplete_coverage(self):
        for change in ({"interval_samples": 3400}, {"observation_count": 3600},
                       {"missing_window_observations": 1}, {"frames_drawn_delta_total": 3598},
                       {"counter_semantics": "display_present"}, {"transition_count": 1}):
            trace = self.window_trace()
            trace["window_render_diagnostics"].update(change)
            with self.subTest(change=change), self.assertRaises(RuntimeError):
                self.validate_trace(trace)
        for field in ("window_drawable", "window_focused", "frames_drawn"):
            trace = self.window_trace(False)
            del trace["maximum_context"]["entry"][field]
            with self.subTest(field=field), self.assertRaises(RuntimeError):
                self.validate_trace(trace)
        trace = self.window_trace(False)
        trace["maximum_context"]["next_window"]["window_drawable"] = 1
        with self.assertRaisesRegex(RuntimeError, "non-boolean"):
            self.validate_trace(trace)

    def test_window_trace_rejects_dropped_groups_fabricated_times_or_missing_maximum(self):
        for change in ({"samples": 3500}, {"maximum_ms": 40}, {"slow_count": 0},
                       {"p95_ms": float("nan")}, {"p99_ms": 2000}):
            trace = self.window_trace()
            trace["window_render_diagnostics"]["categories"]["draw_advanced_focused_drawable"].update(change)
            with self.subTest(change=change), self.assertRaises(RuntimeError):
                self.validate_trace(trace)
        trace = self.window_trace()
        trace["window_render_diagnostics"]["categories"]["no_draw_advance"]["p95_ms"] = 1
        with self.assertRaisesRegex(RuntimeError, "empty category"):
            self.validate_trace(trace)

    def test_zero_or_reset_draw_counter_cannot_qualify_as_rendering(self):
        for next_count in (12, 0):
            trace = self.window_trace(False)
            trace["maximum_context"]["next_window"]["frames_drawn"] = next_count
            with self.subTest(next_count=next_count), self.assertRaisesRegex(RuntimeError, "classified"):
                self.validate_trace(trace)

    def test_no_draw_interval_is_preserved_but_excluded_from_qualified_timing(self):
        for counter_reset in (False, True):
            trace = self.window_trace()
            group_name = "counter_unavailable" if counter_reset else "no_draw_advance"
            for row in trace["first_slow"] + trace["worst_slow"] + [trace["maximum_context"]]:
                if row["callback"] != 12:
                    continue
                row["next_window"]["frames_drawn"] = 0 if counter_reset else 12
                row["window_interval"].update(category=group_name, frames_drawn_delta=None if counter_reset else 0)
            diagnostics = trace["window_render_diagnostics"]
            diagnostics["categories"]["draw_advanced_focused_drawable"].update(samples=3598, slow_count=1, maximum_ms=43)
            diagnostics["categories"][group_name] = {"samples": 1, "slow_count": 1, "maximum_ms": 1061,
                                                    "p50_ms": 1061, "p95_ms": 1061, "p99_ms": 1061}
            diagnostics["frames_drawn_delta_total"] = 3598
            report = self.validate_trace(trace)
            summary = BENCH.draw_observation_summary(report)
            self.assertEqual(summary["excluded_intervals"], 1)
            self.assertEqual(summary["focused_drawable_draw_advancing_intervals"]["maximum_ms"], 43)

    def test_existing_geometry_counter_cannot_certify_all_occluded_callbacks(self):
        trace = self.window_trace()
        for row in trace["first_slow"] + trace["worst_slow"] + [trace["maximum_context"]]:
            row["next_window"]["frames_drawn"] = row["entry"]["frames_drawn"]
            row["window_interval"].update(category="no_draw_advance", frames_drawn_delta=0)
        diagnostics = trace["window_render_diagnostics"]
        groups = diagnostics["categories"]
        groups["no_draw_advance"], groups["draw_advanced_focused_drawable"] = (
            groups["draw_advanced_focused_drawable"], groups["no_draw_advance"])
        diagnostics["frames_drawn_delta_total"] = 0
        with self.assertRaisesRegex(RuntimeError, "no focused/drawable interval"):
            self.validate_trace(trace)

    def test_window_transitions_record_entry_exit_and_reject_impossible_history(self):
        trace = self.window_trace()
        for row in trace["first_slow"] + trace["worst_slow"] + [trace["maximum_context"]]:
            was_focused = row["callback"] == 12
            row["entry"]["window_focused"] = was_focused
            row["exit"]["window_focused"] = not was_focused
            row["next_window"]["window_focused"] = not was_focused
            row["window_interval"].update(category="draw_advanced_other_window_state", focused="changed")
        diagnostics = trace["window_render_diagnostics"]
        diagnostics["categories"]["draw_advanced_focused_drawable"].update(samples=3597, slow_count=0, maximum_ms=16)
        diagnostics["categories"]["draw_advanced_other_window_state"] = {
            "samples": 2, "slow_count": 2, "maximum_ms": 1061, "p50_ms": 1061, "p95_ms": 1061, "p99_ms": 1061}
        diagnostics["focused_interval_counts"].update(all_true=3597, changed=2)
        diagnostics["transition_count"] = 2
        focused = {"window_focused": True, "window_drawable": True}
        unfocused = focused | {"window_focused": False}
        diagnostics["transitions"] = [
            {"observation": 24, "callback": 12, "boundary": "exit", "observed_seconds": 1,
             "before": focused, "after": unfocused},
            {"observation": 4290, "callback": 2145, "boundary": "exit", "observed_seconds": 2,
             "before": unfocused, "after": focused}]
        self.validate_trace(trace)
        for change in ({"callback": 2144}, {"boundary": "entry"}, {"observation": 22},
                       {"observed_seconds": .5}, {"before": focused}):
            broken = copy.deepcopy(trace)
            broken["window_render_diagnostics"]["transitions"][1].update(change)
            with self.subTest(change=change), self.assertRaises(RuntimeError):
                self.validate_trace(broken)

    def test_focus_loss_inside_callback_cannot_be_counted_as_qualified(self):
        trace = self.window_trace(False)
        trace["maximum_context"]["exit"]["window_focused"] = False
        with self.assertRaisesRegex(RuntimeError, "classified"):
            self.validate_trace(trace)


if __name__ == "__main__":
    unittest.main()
