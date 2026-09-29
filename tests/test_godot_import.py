"""The optional renderer gate must reject silent editor failures."""

import importlib.util
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("godot_import_gate", ROOT / "scripts/test_godot_import.py")
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)


class GodotImportGateTests(unittest.TestCase):
    def test_zero_exit_parse_error_fails(self):
        with self.assertRaises(RuntimeError):
            GATE.validate_output("SCRIPT ERROR: Parse Error: unknown member\n", 0, "import")

    def test_zero_exit_extension_error_fails(self):
        with self.assertRaises(RuntimeError):
            GATE.validate_output("ERROR: Failed to load GDExtension: native.gdextension\n", 0, "import")

    def test_native_loader_failure_fails_without_error_prefix(self):
        with self.assertRaises(RuntimeError):
            GATE.validate_output("dlopen: Library not loaded: libtanks_sample.dylib\n", 0, "smoke")

    def test_colored_error_fails(self):
        with self.assertRaises(RuntimeError):
            GATE.validate_output("\x1b[31mERROR:\x1b[0m missing resource\n", 0, "import")

    def test_nonzero_exit_without_text_fails(self):
        with self.assertRaises(RuntimeError):
            GATE.validate_output("", 139, "smoke")

    def test_successful_import_does_not_prove_game_ran(self):
        with self.assertRaises(RuntimeError):
            GATE.validate_report("Godot Engine\n", 180)

    def test_incomplete_input_tape_fails(self):
        report = {"frames": 179, "demo_fixed_step": True, "final_digest": "abc"}
        with self.assertRaises(RuntimeError):
            GATE.validate_report("TANKS_SAMPLE_READY renderer=dummy\nTANKS_SAMPLE_REPORT " + json.dumps(report), 180)

    def test_missing_native_digest_fails(self):
        report = {"frames": 180, "demo_fixed_step": True}
        with self.assertRaises(RuntimeError):
            GATE.validate_report("TANKS_SAMPLE_READY renderer=dummy\nTANKS_SAMPLE_REPORT " + json.dumps(report), 180)

    def test_complete_game_report_passes(self):
        report = {"frames": 180, "demo_fixed_step": True, "final_digest": "abc"}
        text = "TANKS_SAMPLE_READY renderer=dummy\nTANKS_SAMPLE_REPORT " + json.dumps(report)
        self.assertEqual(GATE.validate_report(GATE.validate_output(text, 0, "smoke"), 180), report)

    def test_ui_success_requires_native_checks(self):
        with self.assertRaises(RuntimeError):
            GATE.validate_ui_report("Godot Engine\n")
        with self.assertRaises(RuntimeError):
            GATE.validate_ui_report("TANKS_UI_CHECKS_PASSED settings/menu\n")
        with self.assertRaises(RuntimeError):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/camera/quick-pause/pixel/menu\n")
        with self.assertRaises(RuntimeError):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/enter-start/render-cache\n")
        self.assertEqual(len(GATE.validate_ui_report(
            "TANKS_UI_CHECKS_PASSED settings/nations/two-player/camera/quick-pause/pixel/menu/"
            "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
            "focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/game-over/record/record-timeout/session-record/"
            "audio-resources/native-audio/coop-camera/player-visibility/running-gear-lifecycle/native-fire-effects/arcade-hud/raylib-ui-parity\n")), 28)

    def test_ui_requires_raylib_presentation_parity(self):
        with self.assertRaisesRegex(RuntimeError, "all required"):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/coop-camera/player-visibility/running-gear-lifecycle/native-fire-effects/arcade-hud\n")

    def test_ui_requires_arcade_hud_integration(self):
        with self.assertRaisesRegex(RuntimeError, "all required"):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/coop-camera/player-visibility/running-gear-lifecycle/native-fire-effects/raylib-ui-parity\n")

    def test_ui_requires_real_native_fire_integration(self):
        with self.assertRaisesRegex(RuntimeError, "all required"):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/coop-camera/player-visibility/running-gear-lifecycle/arcade-hud/raylib-ui-parity\n")

    def test_ui_requires_real_player_visibility_integration(self):
        with self.assertRaisesRegex(RuntimeError, "all required"):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/coop-camera/running-gear-lifecycle/native-fire-effects/arcade-hud/raylib-ui-parity\n")

    def test_ui_requires_real_coop_camera_integration(self):
        with self.assertRaisesRegex(RuntimeError, "all required"):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/player-visibility/running-gear-lifecycle/native-fire-effects/arcade-hud/raylib-ui-parity\n")

    def test_ui_requires_focus_lifecycle_contract(self):
        with self.assertRaisesRegex(RuntimeError, "all required"):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/background-gui/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/coop-camera/player-visibility/running-gear-lifecycle/native-fire-effects/arcade-hud/raylib-ui-parity\n")

    def test_ui_requires_background_gui_dispatch_contract(self):
        with self.assertRaisesRegex(RuntimeError, "all required"):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/coop-camera/player-visibility/running-gear-lifecycle/native-fire-effects/arcade-hud/raylib-ui-parity\n")

    def test_ui_requires_running_gear_lifecycle(self):
        text = ("TANKS_UI_CHECKS_PASSED settings/nations/two-player/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/coop-camera/player-visibility/native-fire-effects/arcade-hud/raylib-ui-parity\n")
        with self.assertRaisesRegex(RuntimeError, "all required"):
            GATE.validate_ui_report(text)

    def gear_report(self):
        return {"status": "passed", "models": 24, "cases": 493, "geometry_frames": 768,
                "maximum_triangles": 3788,
                "checks": ["displacement", "blocked", "cardinals", "reverse", "turn", "lateral", "pause", "freeze",
                           "creating", "teleport", "respawn", "isolation", "shared-mesh", "bounds", "budgets",
                           "ghost-shadows", "boat", "reset", "catchup", "rng", "historical-chassis"]}

    def test_running_gear_requires_unique_valid_report(self):
        prefix = "TANKS_RUNNING_GEAR_PASSED "
        line = prefix + json.dumps(self.gear_report()) + "\n"
        self.assertEqual(GATE.validate_running_gear_report(line), self.gear_report())
        for text in ("", line + line, prefix, prefix + "{", prefix + "[]", prefix + "null"):
            with self.subTest(text=text), self.assertRaises(RuntimeError):
                GATE.validate_running_gear_report(text)

    def test_running_gear_requires_all_contracts(self):
        required = self.gear_report()["checks"]
        invalid = [required[:i] + required[i + 1:] for i in range(len(required))]
        invalid += [None, required + ["rng"], required[:-1] + [{}], required[:-1] + ["unknown"]]
        for checks in invalid:
            with self.subTest(checks=checks), self.assertRaises(RuntimeError):
                GATE.validate_running_gear_report("TANKS_RUNNING_GEAR_PASSED " + json.dumps(self.gear_report() | {"checks": checks}))

    def test_running_gear_requires_exact_counts_and_budget(self):
        mutations = []
        for key in ("models", "cases", "geometry_frames"):
            count = self.gear_report()[key]
            mutations += [{key: value} for value in (None, True, str(count), float(count), count - 1, count + 1)]
        mutations += [{"maximum_triangles": value} for value in (None, True, 0, -1, 4001, 3788.0, "3788")]
        mutations += [{"status": value} for value in (None, True, "failed")]
        for mutation in mutations:
            with self.subTest(mutation=mutation), self.assertRaises(RuntimeError):
                GATE.validate_running_gear_report("TANKS_RUNNING_GEAR_PASSED " + json.dumps(self.gear_report() | mutation))

    def test_trace_requires_bounded_window_evidence(self):
        old = "TANKS_FRAME_TRACE_PASSED prior-callback/all-phases/bounds/ownership/counter-reset/finalization"
        with self.assertRaisesRegex(RuntimeError, "bounded window"):
            GATE.validate_trace_report(old)
        complete = old + "/window-draw-coverage/window-transition-bounds\n"
        self.assertEqual(len(GATE.validate_trace_report(complete)), 8)
        with self.assertRaisesRegex(RuntimeError, "bounded window"):
            GATE.validate_trace_report(complete + complete)

    def coop_camera_marker(self):
        return ("TANKS_COOP_CAMERA_PASSED projected-bounds/road-margin/minimum-fit/"
                "orbit/solo-nearby/order/lag/aspect cases=135\n")

    def test_coop_camera_requires_actual_completed_projection_checks(self):
        with self.assertRaisesRegex(RuntimeError, "exactly one"):
            GATE.validate_coop_camera_report("Godot Engine\n")
        marker = self.coop_camera_marker()
        report = GATE.validate_coop_camera_report("Godot Engine\n" + marker)
        self.assertEqual(report["cases"], 135)
        self.assertEqual(len(report["checks"]), 8)

    def test_coop_camera_rejects_each_missing_contract(self):
        checks = self.coop_camera_marker().split(" ")[1].split("/")
        for omitted in checks:
            incomplete = "/".join(check for check in checks if check != omitted)
            with self.subTest(omitted=omitted), self.assertRaisesRegex(RuntimeError, "all projection contracts"):
                GATE.validate_coop_camera_report("TANKS_COOP_CAMERA_PASSED " + incomplete + " cases=135\n")

    def test_coop_camera_rejects_missing_or_inexact_case_count(self):
        for suffix in ("", " cases=134", " cases=136", " cases=135.0", " cases=0135", " cases=135 extra"):
            text = self.coop_camera_marker().split(" cases=")[0] + suffix + "\n"
            with self.subTest(suffix=suffix), self.assertRaisesRegex(RuntimeError, "exactly 135"):
                GATE.validate_coop_camera_report(text)
        with self.assertRaisesRegex(RuntimeError, "exactly 135"):
            GATE.validate_coop_camera_report("TANKS_COOP_CAMERA_PASSED\n")

    def test_coop_camera_rejects_duplicate_completion_and_duplicate_contract(self):
        marker = self.coop_camera_marker()
        for text in (marker + marker, marker + "TANKS_COOP_CAMERA_PASSED\n"):
            with self.subTest(text=text), self.assertRaisesRegex(RuntimeError, "exactly one"):
                GATE.validate_coop_camera_report(text)
        with self.assertRaisesRegex(RuntimeError, "all projection contracts"):
            GATE.validate_coop_camera_report(marker.replace("/aspect", "/aspect/aspect"))

    def battlefield_camera_report(self):
        return {"status": "passed", "cases": 1809,
                "checks": ["only-translation", "player-envelope", "road-margin", "map-coverage", "centering",
                           "angles", "order", "continuity", "lag", "near-depth", "no-players"]}

    def test_battlefield_camera_requires_one_completed_json_report(self):
        prefix = "TANKS_BATTLEFIELD_CAMERA_PASSED "
        report = self.battlefield_camera_report()
        line = prefix + json.dumps(report) + "\n"
        self.assertEqual(GATE.validate_battlefield_camera_report("Godot Engine\n" + line), report)
        for text in ("Godot Engine\n", line + line, line + prefix.rstrip() + "\n"):
            with self.subTest(text=text), self.assertRaisesRegex(RuntimeError, "exactly one"):
                GATE.validate_battlefield_camera_report(text)
        for payload in ("", "{", "[]", "null", json.dumps(report) + " trailing"):
            with self.subTest(payload=payload), self.assertRaisesRegex(RuntimeError, "JSON"):
                GATE.validate_battlefield_camera_report(prefix + payload + "\n")

    def test_battlefield_camera_requires_every_contract_without_duplicates(self):
        for omitted in self.battlefield_camera_report()["checks"]:
            report = self.battlefield_camera_report()
            report["checks"].remove(omitted)
            with self.subTest(omitted=omitted), self.assertRaisesRegex(RuntimeError, "11 unique"):
                GATE.validate_battlefield_camera_report("TANKS_BATTLEFIELD_CAMERA_PASSED " + json.dumps(report))
        for invalid in (None, {}, "only-translation", ["only-translation"] * 11,
                        self.battlefield_camera_report()["checks"] + ["lag"],
                        self.battlefield_camera_report()["checks"][:-1] + [{}],
                        self.battlefield_camera_report()["checks"][:-1] + ["unknown"]):
            report = self.battlefield_camera_report() | {"checks": invalid}
            with self.subTest(invalid=invalid), self.assertRaisesRegex(RuntimeError, "11 unique"):
                GATE.validate_battlefield_camera_report("TANKS_BATTLEFIELD_CAMERA_PASSED " + json.dumps(report))

    def test_battlefield_camera_requires_exact_integer_cases_and_passed_status(self):
        for invalid in (None, True, 1808, 1810, 1809.0, "1809"):
            report = self.battlefield_camera_report() | {"cases": invalid}
            with self.subTest(invalid=invalid), self.assertRaisesRegex(RuntimeError, "exactly 1809"):
                GATE.validate_battlefield_camera_report("TANKS_BATTLEFIELD_CAMERA_PASSED " + json.dumps(report))
        for status in (None, False, "failed", "passed-with-errors"):
            report = self.battlefield_camera_report() | {"status": status}
            with self.subTest(status=status), self.assertRaisesRegex(RuntimeError, "exactly 1809"):
                GATE.validate_battlefield_camera_report("TANKS_BATTLEFIELD_CAMERA_PASSED " + json.dumps(report))

    def visibility_report(self):
        return {"status": "passed", "cases": 30,
                "checks": ["brick-mask", "occluder-height", "forest-cover", "player-only", "rigid-follow",
                           "material-isolation", "source-unchanged", "lifetime", "bounded-cache", "pause", "rng"]}

    def test_player_visibility_requires_unique_valid_completed_report(self):
        prefix = "TANKS_PLAYER_VISIBILITY_PASSED "
        report = self.visibility_report()
        marker = prefix + json.dumps(report) + "\n"
        self.assertEqual(GATE.validate_player_visibility_report(marker), report)
        for text in ("Godot Engine\n", marker + marker):
            with self.subTest(text=text), self.assertRaisesRegex(RuntimeError, "exactly one"):
                GATE.validate_player_visibility_report(text)
        for invalid in ("", "{", "[]", "null"):
            with self.subTest(invalid=invalid), self.assertRaisesRegex(RuntimeError, "JSON"):
                GATE.validate_player_visibility_report(prefix + invalid)

    def test_player_visibility_rejects_missing_or_duplicate_contract(self):
        reports = []
        for omitted in self.visibility_report()["checks"]:
            value = self.visibility_report()
            value["checks"].remove(omitted)
            reports.append(value)
        for invalid in (None, {}, "forest-cover", ["forest-cover"] * 11,
                        self.visibility_report()["checks"] + ["rng"],
                        self.visibility_report()["checks"][:-1] + [{}]):
            reports.append(self.visibility_report() | {"checks": invalid})
        for report in reports:
            with self.subTest(report=report), self.assertRaisesRegex(RuntimeError, "11 unique"):
                GATE.validate_player_visibility_report("TANKS_PLAYER_VISIBILITY_PASSED " + json.dumps(report))

    def test_player_visibility_requires_exact_count_and_passed_status(self):
        for invalid in (None, True, 29, 31, 30.0, "30"):
            report = self.visibility_report() | {"cases": invalid}
            with self.subTest(invalid=invalid), self.assertRaisesRegex(RuntimeError, "exactly 30"):
                GATE.validate_player_visibility_report("TANKS_PLAYER_VISIBILITY_PASSED " + json.dumps(report))
        for status in (None, False, "failed"):
            report = self.visibility_report() | {"status": status}
            with self.subTest(status=status), self.assertRaisesRegex(RuntimeError, "exactly 30"):
                GATE.validate_player_visibility_report("TANKS_PLAYER_VISIBILITY_PASSED " + json.dumps(report))

    def art_report(self):
        return {"status": "passed", "counts": {"vehicles": 24, "national_color_cases": 192, "brick_cases": 512,
            "pickups": 9, "base_walls": 10, "bases": 3, "forest_cases": 676, "motion_cases": 24}, "meshes_checked": 200,
            "checks": ["geometry", "winding", "budgets", "cache", "material-isolation",
                       "identity", "national-color", "enemy-status-patches", "muzzle", "footprints", "motion", "effects", "rng"]}

    def test_art_parse_success_cannot_replace_contract_execution(self):
        with self.assertRaisesRegex(RuntimeError, "exactly one"):
            GATE.validate_art_report("Godot Engine\n")
        line = "TANKS_ART_CHECKS_PASSED " + json.dumps(self.art_report()) + "\n"
        with self.assertRaisesRegex(RuntimeError, "exactly one"):
            GATE.validate_art_report(line + line)

    def test_incomplete_art_coverage_is_rejected(self):
        report = self.art_report()
        report["counts"]["vehicles"] = 12
        with self.assertRaisesRegex(RuntimeError, "every required"):
            GATE.validate_art_report("TANKS_ART_CHECKS_PASSED " + json.dumps(report))
        report = self.art_report()
        report["checks"].remove("material-isolation")
        with self.assertRaisesRegex(RuntimeError, "every required"):
            GATE.validate_art_report("TANKS_ART_CHECKS_PASSED " + json.dumps(report))

    def test_incomplete_forest_coverage_is_rejected(self):
        report = self.art_report()
        report["counts"]["forest_cases"] = 16
        with self.assertRaisesRegex(RuntimeError, "every required"):
            GATE.validate_art_report("TANKS_ART_CHECKS_PASSED " + json.dumps(report))

    def test_static_art_cannot_replace_motion_coverage(self):
        for key in ("missing-count", "partial-count", "missing-check"):
            report = self.art_report()
            if key == "missing-count":
                del report["counts"]["motion_cases"]
            elif key == "partial-count":
                report["counts"]["motion_cases"] = 12
            else:
                report["checks"].remove("motion")
            with self.assertRaisesRegex(RuntimeError, "every required"):
                GATE.validate_art_report("TANKS_ART_CHECKS_PASSED " + json.dumps(report))

    def test_art_requires_every_national_color_state(self):
        for invalid in (None, 0, 24, 96, 191, 193, True, 192.0, "192"):
            report = self.art_report()
            if invalid is None:
                del report["counts"]["national_color_cases"]
            else:
                report["counts"]["national_color_cases"] = invalid
            with self.subTest(invalid=invalid), self.assertRaisesRegex(RuntimeError, "every required"):
                GATE.validate_art_report("TANKS_ART_CHECKS_PASSED " + json.dumps(report))

    def test_art_requires_national_hue_and_enemy_patch_contracts(self):
        for omitted in ("national-color", "enemy-status-patches"):
            report = self.art_report()
            report["checks"].remove(omitted)
            with self.subTest(omitted=omitted), self.assertRaisesRegex(RuntimeError, "every required"):
                GATE.validate_art_report("TANKS_ART_CHECKS_PASSED " + json.dumps(report))

    def test_complete_structural_art_report_passes(self):
        report = self.art_report()
        self.assertEqual(GATE.validate_art_report("TANKS_ART_CHECKS_PASSED " + json.dumps(report)), report)

    def test_audio_import_success_does_not_prove_resources_loaded(self):
        with self.assertRaisesRegex(RuntimeError, "exactly one"):
            GATE.validate_audio_report("Godot Engine\n")

    def test_missing_audio_cue_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "every native cue"):
            GATE.validate_audio_report('TANKS_AUDIO_CHECKS_PASSED {"resources":21,"checks":["resources"]}')

    def test_complete_audio_resource_report_passes(self):
        report = {"resources": 22, "checks": ["resources"], "mixed_cues": {}}
        self.assertEqual(GATE.validate_audio_report("TANKS_AUDIO_CHECKS_PASSED " + json.dumps(report)), report)

    def effect_pool_report(self):
        return {"status": "passed", "live_limit": 48, "retained_limit": 96,
                "warmed_created": 96, "final_created": 96, "retained_peak": 96,
                "cycles": 192, "reused": 9234,
                "checks": ["lazy", "admission", "reuse", "reset", "visibility",
                           "material-isolation", "shared-mesh", "lifecycle", "integration", "rng"]}

    def test_effect_pool_parse_success_does_not_prove_lifecycle_checks(self):
        with self.assertRaisesRegex(RuntimeError, "exactly one"):
            GATE.validate_effect_pool_report("Godot Engine\n")
        line = "TANKS_EFFECT_POOL_CHECKS_PASSED " + json.dumps(self.effect_pool_report()) + "\n"
        with self.assertRaisesRegex(RuntimeError, "exactly one"):
            GATE.validate_effect_pool_report(line + line)

    def test_effect_pool_rejects_growth_after_warmup(self):
        for field, value in [("live_limit", 49), ("retained_peak", 97),
                             ("final_created", 97), ("cycles", 191), ("reused", 2)]:
            with self.subTest(field=field):
                report = self.effect_pool_report()
                report[field] = value
                with self.assertRaisesRegex(RuntimeError, "bounds, reuse"):
                    GATE.validate_effect_pool_report("TANKS_EFFECT_POOL_CHECKS_PASSED " + json.dumps(report))

    def test_effect_pool_requires_application_and_material_contracts(self):
        for omitted in ["integration", "material-isolation", "reset", "lifecycle"]:
            with self.subTest(omitted=omitted):
                report = self.effect_pool_report()
                report["checks"].remove(omitted)
                with self.assertRaisesRegex(RuntimeError, "visual lifecycle"):
                    GATE.validate_effect_pool_report("TANKS_EFFECT_POOL_CHECKS_PASSED " + json.dumps(report))

    def test_complete_effect_pool_report_passes(self):
        report = self.effect_pool_report()
        self.assertEqual(GATE.validate_effect_pool_report(
            "TANKS_EFFECT_POOL_CHECKS_PASSED " + json.dumps(report)), report)


if __name__ == "__main__":
    unittest.main()
