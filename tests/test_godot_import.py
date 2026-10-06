"""The optional renderer gate must reject silent editor failures."""

import importlib.util
import hashlib
import json
import os
from pathlib import Path
import re
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("godot_import_gate", ROOT / "scripts/test_godot_import.py")
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)

AUDIO_POLICY_CHECKS = ["resources", "lazy-pool", "reuse", "voice-limit", "single",
                       "priority", "engine-exclusive", "disabled", "zero-gain"]
AUDIO_MIXER_CHECKS = ["decoded-mixer", "routing", "overlap-limit", "zero-mute",
                      "volume-restore", "stop"]


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
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/controller-menu/frame-input/camera/quick-pause/pixel/menu\n")
        with self.assertRaises(RuntimeError):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/controller-menu/frame-input/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/enter-start/render-cache\n")
        self.assertEqual(len(GATE.validate_ui_report(
            "TANKS_UI_CHECKS_PASSED settings/nations/two-player/controller-menu/frame-input/camera/quick-pause/pixel/menu/"
            "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
            "focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/game-over/record/record-timeout/session-record/"
            "audio-resources/native-audio/coop-camera/player-visibility/running-gear-lifecycle/native-fire-effects/arcade-hud/raylib-ui-parity\n")), 30)

    def test_ui_requires_controller_menu_and_same_frame_input(self):
        text = ("TANKS_UI_CHECKS_PASSED settings/nations/two-player/controller-menu/frame-input/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/coop-camera/player-visibility/running-gear-lifecycle/native-fire-effects/arcade-hud/raylib-ui-parity\n")
        for required in ["controller-menu", "frame-input"]:
            with self.subTest(required=required), self.assertRaisesRegex(RuntimeError, "all required"):
                GATE.validate_ui_report(text.replace(required + "/", ""))

    def test_ui_requires_raylib_presentation_parity(self):
        with self.assertRaisesRegex(RuntimeError, "all required"):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/controller-menu/frame-input/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/coop-camera/player-visibility/running-gear-lifecycle/native-fire-effects/arcade-hud\n")

    def test_ui_requires_arcade_hud_integration(self):
        with self.assertRaisesRegex(RuntimeError, "all required"):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/controller-menu/frame-input/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/coop-camera/player-visibility/running-gear-lifecycle/native-fire-effects/raylib-ui-parity\n")

    def test_ui_requires_real_native_fire_integration(self):
        with self.assertRaisesRegex(RuntimeError, "all required"):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/controller-menu/frame-input/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/coop-camera/player-visibility/running-gear-lifecycle/arcade-hud/raylib-ui-parity\n")

    def test_ui_requires_real_player_visibility_integration(self):
        with self.assertRaisesRegex(RuntimeError, "all required"):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/controller-menu/frame-input/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/coop-camera/running-gear-lifecycle/native-fire-effects/arcade-hud/raylib-ui-parity\n")

    def test_ui_requires_real_coop_camera_integration(self):
        with self.assertRaisesRegex(RuntimeError, "all required"):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/controller-menu/frame-input/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/player-visibility/running-gear-lifecycle/native-fire-effects/arcade-hud/raylib-ui-parity\n")

    def test_ui_requires_focus_lifecycle_contract(self):
        with self.assertRaisesRegex(RuntimeError, "all required"):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/controller-menu/frame-input/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/background-gui/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/coop-camera/player-visibility/running-gear-lifecycle/native-fire-effects/arcade-hud/raylib-ui-parity\n")

    def test_ui_requires_background_gui_dispatch_contract(self):
        with self.assertRaisesRegex(RuntimeError, "all required"):
            GATE.validate_ui_report(
                "TANKS_UI_CHECKS_PASSED settings/nations/two-player/controller-menu/frame-input/camera/quick-pause/pixel/menu/"
                "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                "focus-clear/enter-start/render-cache/game-over/record/record-timeout/session-record/"
                "audio-resources/native-audio/coop-camera/player-visibility/running-gear-lifecycle/native-fire-effects/arcade-hud/raylib-ui-parity\n")

    def test_ui_requires_running_gear_lifecycle(self):
        text = ("TANKS_UI_CHECKS_PASSED settings/nations/two-player/controller-menu/frame-input/camera/quick-pause/pixel/menu/"
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
        report = {"resources": 22, "checks": AUDIO_POLICY_CHECKS, "mixed_cues": {}}
        self.assertEqual(GATE.validate_audio_report("TANKS_AUDIO_CHECKS_PASSED " + json.dumps(report)), report)

    def test_audio_requires_each_pool_contract(self):
        for missing in AUDIO_POLICY_CHECKS:
            report = {"resources": 22, "checks": [value for value in AUDIO_POLICY_CHECKS if value != missing]}
            with self.subTest(missing=missing), self.assertRaisesRegex(RuntimeError, "every lazy"):
                GATE.validate_audio_report("TANKS_AUDIO_CHECKS_PASSED " + json.dumps(report))

    def test_audio_rejects_duplicate_or_invalid_contracts(self):
        for checks in (None, "resources", AUDIO_POLICY_CHECKS + ["resources"], AUDIO_POLICY_CHECKS + [True]):
            with self.subTest(checks=checks), self.assertRaisesRegex(RuntimeError, "every lazy"):
                GATE.validate_audio_report("TANKS_AUDIO_CHECKS_PASSED " + json.dumps({"resources": 22, "checks": checks}))

    def test_audio_rejects_malformed_or_repeated_reports(self):
        with self.assertRaisesRegex(RuntimeError, "valid JSON"):
            GATE.validate_audio_report("TANKS_AUDIO_CHECKS_PASSED {")
        for report in ([], None, {"resources": 22.0}, {"resources": True}):
            with self.subTest(report=report), self.assertRaisesRegex(RuntimeError, "every native cue"):
                GATE.validate_audio_report("TANKS_AUDIO_CHECKS_PASSED " + json.dumps(report))
        line = "TANKS_AUDIO_CHECKS_PASSED " + json.dumps({"resources": 22, "checks": AUDIO_POLICY_CHECKS}) + "\n"
        with self.assertRaisesRegex(RuntimeError, "exactly one"):
            GATE.validate_audio_report(line + line)

    def mixer_report(self):
        return {"resources": 22, "checks": AUDIO_POLICY_CHECKS + AUDIO_MIXER_CHECKS,
                "mixed_cues": {path.stem: 0.125 for path in (ROOT / "resources/sounds").glob("*.ogg")}}

    def test_complete_decoded_audio_report_passes(self):
        report = self.mixer_report()
        self.assertEqual(GATE.validate_audio_report("TANKS_AUDIO_CHECKS_PASSED " + json.dumps(report),
                                                  require_mixer=True), report)

    def test_audio_mixer_rejects_resource_only_or_partial_reports(self):
        report = self.mixer_report()
        partial = [{"resources": 22, "checks": AUDIO_POLICY_CHECKS, "mixed_cues": {}}]
        for missing in AUDIO_MIXER_CHECKS:
            partial.append(dict(report, checks=[value for value in report["checks"] if value != missing]))
        cues = dict(report["mixed_cues"])
        cues.pop(next(iter(cues)))
        partial.append(dict(report, mixed_cues=cues))
        wrong_cues = dict(cues, unknown_cue=0.125)
        partial.append(dict(report, mixed_cues=wrong_cues))
        for invalid in partial:
            with self.subTest(report=invalid), self.assertRaisesRegex(RuntimeError, "decoded samples"):
                GATE.validate_audio_report("TANKS_AUDIO_CHECKS_PASSED " + json.dumps(invalid), require_mixer=True)

    def test_audio_mixer_rejects_silent_or_invalid_samples(self):
        for sample in (0, -0.1, 0.000001, True, None, "0.125", float("nan"), float("inf")):
            report = self.mixer_report()
            report["mixed_cues"][next(iter(report["mixed_cues"]))] = sample
            with self.subTest(sample=sample), self.assertRaisesRegex(RuntimeError, "decoded samples"):
                GATE.validate_audio_report("TANKS_AUDIO_CHECKS_PASSED " + json.dumps(report), require_mixer=True)

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


class GodotPreparationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="godot-preparation-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.project = self.root / "build/project"
        self.logs = self.root / "build/logs"
        self.project.mkdir(parents=True)
        self.logs.mkdir(parents=True)
        self.engine = self.root / "engine-bin"
        self.engine.write_bytes(b"engine-v1")
        self.dependencies = {"godot": {"sha256": "pinned-toolchain"}}
        self.staged = {"main.gd": "script-v1", "native/libtanks_sample.dylib": "native-v1"}
        self.addCleanup(mock.patch.stopall)
        mock.patch.object(GATE, "ROOT", self.root).start()
        mock.patch.object(GATE, "DEFAULT_PROJECT", self.project).start()

    def write_products(self, *args):
        imported = self.project / ".godot/imported"
        imported.mkdir(parents=True, exist_ok=True)
        (imported / "texture.ctex").write_bytes(b"imported-texture")
        for name in ("extension_list.cfg", "global_script_class_cache.cfg", "uid_cache.bin"):
            (self.project / ".godot" / name).write_bytes(b"import-metadata")
        (self.project / "main.gd.uid").write_bytes(b"script-uid")
        return ""

    def prepare(self, force=False):
        GATE.prepare_import(self.engine, self.project, self.logs, 120,
                            self.dependencies, self.staged, force=force)

    def test_unchanged_preparation_reuses_verified_import(self):
        with mock.patch.object(GATE, "run_check", side_effect=self.write_products) as run:
            self.prepare()
            self.prepare()
        self.assertEqual(run.call_count, 1)
        self.assertFalse((self.logs / "result.json").exists())
        receipt = json.loads((self.logs / "prepare-result.json").read_text())
        self.assertEqual(receipt["scope"], "staging/import")

    def test_engine_inputs_and_import_products_invalidate_preparation(self):
        def change_staged(name):
            self.staged[name] += "-changed"

        mutations = {
            "engine": lambda: self.engine.write_bytes(b"engine-v2"),
            "script": lambda: change_staged("main.gd"),
            "native": lambda: change_staged("native/libtanks_sample.dylib"),
            "imported-resource": lambda: (self.project / ".godot/imported/texture.ctex").write_bytes(b"corrupt"),
            "missing-resource": lambda: (self.project / ".godot/imported/texture.ctex").unlink(),
            "missing-discovery": lambda: (self.project / ".godot/extension_list.cfg").unlink(),
            "uid": lambda: (self.project / "main.gd.uid").write_bytes(b"changed-uid"),
            "invalid-receipt": lambda: (self.logs / "prepare-result.json").write_text("{"),
        }
        with mock.patch.object(GATE, "run_check", side_effect=self.write_products) as run:
            self.prepare()
            for name, mutate in mutations.items():
                with self.subTest(name=name):
                    before = run.call_count
                    mutate()
                    self.prepare()
                    self.assertEqual(run.call_count, before + 1)
                    self.prepare()
                    self.assertEqual(run.call_count, before + 1)

    def test_failed_forced_import_cannot_reuse_a_previous_receipt(self):
        with mock.patch.object(GATE, "run_check", side_effect=self.write_products):
            self.prepare()
        with mock.patch.object(GATE, "run_check", side_effect=RuntimeError("parse failure")):
            with self.assertRaisesRegex(RuntimeError, "parse failure"):
                self.prepare(force=True)
        self.assertFalse((self.logs / "prepare-result.json").exists())
        with mock.patch.object(GATE, "run_check", side_effect=self.write_products) as run:
            self.prepare()
            self.assertEqual(run.call_count, 1)

    def test_old_receipt_cannot_reuse_potentially_stale_imports(self):
        with mock.patch.object(GATE, "run_check", side_effect=self.write_products) as run:
            self.prepare()
            receipt = self.logs / "prepare-result.json"
            previous = json.loads(receipt.read_text())
            previous["schema"] = "tanks3d-godot-preparation-v1"
            receipt.write_text(json.dumps(previous))
            self.prepare()
            self.assertEqual(run.call_count, 2)
            self.prepare()
            self.assertEqual(run.call_count, 2)

    def test_linked_editor_cache_fails_without_writing_a_receipt(self):
        editor = self.project / ".godot/editor"
        editor.mkdir(parents=True)
        outside = self.root / "outside-cache"
        outside.write_text("external")
        (editor / "filesystem_cache10").symlink_to(outside)
        with self.assertRaisesRegex(RuntimeError, "Linked editor filesystem cache"):
            self.prepare()
        self.assertEqual(outside.read_text(), "external")
        self.assertFalse((self.logs / "prepare-result.json").exists())

    def test_linked_editor_directory_cannot_delete_external_caches(self):
        outside = self.root / "outside-editor"
        outside.mkdir()
        cache = outside / "filesystem_cache10"
        cache.write_text("external")
        (self.project / ".godot").mkdir()
        (self.project / ".godot/editor").symlink_to(outside, target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError, "Linked editor cache directory"):
            self.prepare()
        self.assertEqual(cache.read_text(), "external")

    def test_missing_products_after_successful_exit_are_rejected(self):
        with mock.patch.object(GATE, "run_check", return_value=""):
            with self.assertRaisesRegex(RuntimeError, "required discovery"):
                self.prepare()
        self.assertFalse((self.logs / "prepare-result.json").exists())

    def engine_output(self, godot, project, logs, label, arguments, timeout):
        reports = GodotImportGateTests()
        values = {
            "art-contracts": "TANKS_ART_CHECKS_PASSED " + json.dumps(reports.art_report()),
            "shell-flight": "TANKS_SHELL_FLIGHT_CHECKS_PASSED 12-models/four-directions/owner-lifecycle/native-impact",
            "coop-camera": reports.coop_camera_marker(),
            "battlefield-camera": "TANKS_BATTLEFIELD_CAMERA_PASSED " + json.dumps(reports.battlefield_camera_report()),
            "player-visibility": "TANKS_PLAYER_VISIBILITY_PASSED " + json.dumps(reports.visibility_report()),
            "running-gear": "TANKS_RUNNING_GEAR_PASSED " + json.dumps(reports.gear_report()),
            "effect-pool": "TANKS_EFFECT_POOL_CHECKS_PASSED " + json.dumps(reports.effect_pool_report()),
            "frame-metrics": "TANKS_FRAME_METRICS_PASSED warmup/full-run/windows/bounds/overflow",
            "frame-trace": "TANKS_FRAME_TRACE_PASSED prior-callback/all-phases/bounds/ownership/counter-reset/"
                           "finalization/window-draw-coverage/window-transition-bounds",
            "audio-resources": "TANKS_AUDIO_CHECKS_PASSED " + json.dumps({"resources": 22, "checks": AUDIO_POLICY_CHECKS}),
            "smoke": 'TANKS_SAMPLE_READY renderer=dummy\nTANKS_SAMPLE_REPORT '
                     '{"frames":180,"demo_fixed_step":true,"final_digest":"complete"}',
            "ui-smoke": "TANKS_UI_CHECKS_PASSED settings/nations/two-player/controller-menu/frame-input/camera/"
                        "quick-pause/pixel/menu/restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/"
                        "focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/game-over/record/"
                        "record-timeout/session-record/audio-resources/native-audio/coop-camera/player-visibility/"
                        "running-gear-lifecycle/native-fire-effects/arcade-hud/raylib-ui-parity",
        }
        if label == "import":
            return self.write_products()
        return values.get(label, "")

    def test_preparation_and_both_validation_modes_preserve_their_scope(self):
        sample = self.root / "godot/sample"
        sample.mkdir(parents=True)
        (sample / "main.gd").write_text("extends Node\n")
        (sample / "main.tscn").write_text("[gd_scene format=3]\n")
        arguments = ["--godot", str(self.engine), "--skip-stage", "--log-dir", str(self.logs)]
        contracts = {"art-contracts", "shell-flight", "coop-camera", "battlefield-camera",
                     "player-visibility", "running-gear", "effect-pool", "frame-metrics",
                     "frame-trace", "audio-resources"}
        with mock.patch.object(GATE, "validate_dependencies", return_value=self.dependencies), \
                mock.patch.object(GATE, "validate_staged", return_value=self.staged):
            for mode in ("--prepare-only", "--import-only", None):
                with self.subTest(mode=mode), mock.patch.object(GATE, "run_check", side_effect=self.engine_output) as run:
                    GATE.main(arguments + ([mode] if mode else []))
                    labels = {call.args[3] for call in run.call_args_list}
                    if mode == "--prepare-only":
                        self.assertEqual(labels, {"import"})
                        self.assertFalse((self.logs / "result.json").exists())
                    else:
                        expected = contracts | {"import", "parse-main", "scenes"}
                        if mode is None:
                            expected |= {"smoke", "ui-smoke"}
                        self.assertEqual(labels, expected)
                        self.assertEqual(sum(call.args[3] == "import" for call in run.call_args_list), 1)
                        result = json.loads((self.logs / "result.json").read_text())
                        self.assertEqual(result["status"], "passed")
                        self.assertEqual(result["headless_report"] is None, mode == "--import-only")


@unittest.skipUnless((ROOT / "build/godot-tools/Godot.app/Contents/MacOS/Godot").is_file(),
                     "Requires the pinned engine from make godot-setup")
class GodotRealImportTests(unittest.TestCase):
    def test_same_mtime_edits_and_old_receipts_reimport_resource_content(self):
        # Retain isolated projects/logs under the existing CI diagnostic path.
        logs_root = ROOT / "build/godot/validation"
        logs_root.mkdir(parents=True, exist_ok=True)
        root = Path(tempfile.mkdtemp(prefix="import-cache-", dir=logs_root))
        project = root / "build/godot/project"
        source = root / "source"
        source.mkdir()
        (source / "project.godot").write_text(
            'config_version=5\n[application]\nconfig/name="Import cache regression"\n'
            'config/use_custom_user_dir=true\nconfig/custom_user_dir_name="Tanks3D-Godot"\n')
        (source / "probe.gd").write_text("class_name ImportCacheProbe\nextends RefCounted\n")
        texture = source / "tile.svg"
        texture.write_text('<svg xmlns="http://www.w3.org/2000/svg" width="8" height="8">'
                           '<rect width="8" height="8" fill="blue"/></svg>')
        fixed_time = 1_700_000_000_000_000_000
        os.utime(texture, ns=(fixed_time, fixed_time))
        files = {path.name: path for path in source.iterdir()}
        engine = ROOT / "build/godot-tools/Godot.app/Contents/MacOS/Godot"
        logs = root / "logs"
        logs.mkdir()

        def fixture_products(project, staged):
            # This resource-only fixture has no native discovery metadata.
            # Fingerprint its real Godot products, importer settings and UID;
            # the full game's metadata checks remain unchanged in production.
            paths = list((project / ".godot/imported").glob("*"))
            paths += [project / "tile.svg.import", project / "probe.gd.uid"]
            if any(not path.is_file() for path in paths):
                return None
            return {path.relative_to(project).as_posix(): GATE.digest(path) for path in paths}

        with mock.patch.dict(os.environ, {"SDL_HIDAPI_IGNORE_DEVICES":
                                          os.environ.get("SDL_HIDAPI_IGNORE_DEVICES", "0x057e/0x2009")}), \
                mock.patch.object(GATE, "imported_files", side_effect=fixture_products), \
                mock.patch.object(GATE.staging, "ROOT", root), \
                mock.patch.object(GATE.staging, "TARGET", project), \
                mock.patch.object(GATE.staging, "source_files", return_value=files):
            GATE.staging.main()
            GATE.prepare_import(engine, project, logs, 60, {}, GATE.validate_staged(project))
            imported = next((project / ".godot/imported").glob("tile.svg-*.ctex"))
            md5_record = next((project / ".godot/imported").glob("tile.svg-*.md5"))
            settings = (project / "tile.svg.import").read_bytes()
            script_uid = (project / "probe.gd.uid").read_bytes()
            products = [GATE.digest(imported)]
            for mode, color in (("changed", "lime"), ("old-receipt", "blue"), ("forced", "lime")):
                with self.subTest(mode=mode):
                    texture.write_text(re.sub(r'fill="[a-z]+"', f'fill="{color}"', texture.read_text()))
                    os.utime(texture, ns=(fixed_time, fixed_time))
                    GATE.staging.main()
                    self.assertEqual((project / "tile.svg").stat().st_mtime_ns, fixed_time)
                    staged = GATE.validate_staged(project)
                    receipt = logs / "prepare-result.json"
                    if mode == "old-receipt":
                        # Model an old gate falsely accepting new bytes + old products.
                        previous = json.loads(receipt.read_text())
                        previous.update(schema="tanks3d-godot-preparation-v1", staged_sha256=staged)
                        receipt.write_text(json.dumps(previous))
                    GATE.prepare_import(engine, project, logs, 60, {}, staged, force=mode == "forced")
                    source_md5 = hashlib.md5(texture.read_bytes()).hexdigest()
                    self.assertIn(f'source_md5="{source_md5}"', md5_record.read_text())
                    products.append(GATE.digest(imported))
                    self.assertNotEqual(products[-1], products[-2])
                    self.assertEqual((project / "tile.svg.import").read_bytes(), settings)
                    self.assertEqual((project / "probe.gd.uid").read_bytes(), script_uid)
                    with mock.patch.object(GATE, "run_check") as run:
                        GATE.prepare_import(engine, project, logs, 60, {}, staged)
                        run.assert_not_called()
            self.assertEqual(products[0], products[2])
            self.assertEqual(products[1], products[3])


class GodotStagingTests(unittest.TestCase):
    def test_unchanged_bytes_preserve_destination_mtime_and_changed_bytes_recopy(self):
        with tempfile.TemporaryDirectory(prefix="godot-staging-") as temporary:
            root = Path(temporary).resolve()
            source = root / "source.gd"
            target = root / "build/godot/project"
            source.write_bytes(b"original")
            with mock.patch.object(GATE.staging, "ROOT", root), mock.patch.object(GATE.staging, "TARGET", target), \
                    mock.patch.object(GATE.staging, "source_files", return_value={"main.gd": source}):
                GATE.staging.main()
                destination = target / "main.gd"
                os.utime(destination, ns=(1_000_000_000, 1_000_000_000))
                GATE.staging.main()
                self.assertEqual(destination.stat().st_mtime_ns, 1_000_000_000)
                metadata = source.stat()
                source.write_bytes(b"modified")
                os.utime(source, ns=(metadata.st_atime_ns, metadata.st_mtime_ns))
                GATE.staging.main()
                self.assertEqual(destination.read_bytes(), b"modified")
                destination.write_bytes(b"tampered")
                GATE.staging.main()
                self.assertEqual(destination.read_bytes(), b"modified")

    def test_removed_source_removes_staged_file_and_generated_sidecars(self):
        with tempfile.TemporaryDirectory(prefix="godot-staging-") as temporary:
            root = Path(temporary).resolve()
            source = root / "source.gd"
            target = root / "build/godot/project"
            source.write_text("source")
            files = {"old.gd": source}
            with mock.patch.object(GATE.staging, "ROOT", root), mock.patch.object(GATE.staging, "TARGET", target), \
                    mock.patch.object(GATE.staging, "source_files", return_value=files):
                GATE.staging.main()
                for suffix in (".import", ".uid"):
                    (target / ("old.gd" + suffix)).write_text("generated")
                files.clear()
                GATE.staging.main()
                self.assertFalse(any(target.iterdir()))


if __name__ == "__main__":
    unittest.main()
