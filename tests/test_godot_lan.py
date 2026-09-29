"""The real Godot LAN gate must reject incomplete or divergent evidence."""
import copy
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("godot_lan_gate", ROOT / "scripts/test_godot_lan.py")
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)


class GodotLanGateTests(unittest.TestCase):
    def setUp(self):
        self.reports = {}
        for role, player in [("host", 0), ("guest", 1)]:
            self.reports[role] = {"status": "passed", "role": role, "pid": player + 10,
                "last_tick": 480, "disconnected": True, "disconnect_reason": "peer left",
                "players_moved": [True, True], "player_shots": [4, 20], "local_player": player,
                "input_mask": 17 if player == 0 else 24,
                "settings": {"stage": 1, "players": 2, "ai_p2": False, "lives": 5,
                    "nation_p1": 2, "nation_p2": 1, "max_hp": 6, "enemy_speed": -25,
                    "enemy_fire": 10, "enemy_spawn": -30, "camera_yaw": 25 if player == 0 else -35,
                    "camera_elevation": 60 if player == 0 else 45}}
        trace = {tick: "0123456789abcdef" for tick in range(481)}
        self.traces = {"host": trace, "guest": copy.deepcopy(trace)}

    def test_complete_independent_process_evidence(self):
        self.assertEqual(GATE.validate_pair(self.reports, self.traces)["matched_ticks"], 480)

    def test_any_common_digest_mismatch_fails(self):
        self.traces["guest"][300] = "different"
        with self.assertRaisesRegex(RuntimeError, "tick 300"):
            GATE.validate_pair(self.reports, self.traces)

    def test_sparse_polls_must_still_supply_360_real_shared_observations(self):
        sparse = {tick: "0123456789abcdef" for tick in range(2, 724, 2)}
        self.traces = {"host": dict(sparse), "guest": dict(sparse)}
        for report in self.reports.values():
            report["last_tick"] = 722
        self.assertEqual(GATE.validate_pair(self.reports, self.traces)["matched_ticks"], 361)
        del self.traces["guest"][2]
        del self.traces["guest"][4]
        with self.assertRaisesRegex(RuntimeError, "matched=359"):
            GATE.validate_pair(self.reports, self.traces)

    def test_sufficient_early_samples_cannot_replace_final_shared_progress(self):
        self.traces["host"].pop(480)
        with self.assertRaisesRegex(RuntimeError, "last_common=479"):
            GATE.validate_pair(self.reports, self.traces)

    def test_same_process_cannot_claim_two_peers(self):
        self.reports["guest"]["pid"] = self.reports["host"]["pid"]
        with self.assertRaisesRegex(RuntimeError, "independent"):
            GATE.validate_pair(self.reports, self.traces)

    def test_role_identity_and_both_player_actions_are_required(self):
        for key, value in [("local_player", 0), ("players_moved", [True, False]),
                           ("player_shots", [4, 0]), ("disconnected", False)]:
            reports = copy.deepcopy(self.reports)
            reports["guest"][key] = value
            with self.subTest(key=key), self.assertRaises(RuntimeError):
                GATE.validate_pair(reports, self.traces)

    def test_negotiated_settings_are_required(self):
        self.reports["guest"]["settings"]["nation_p2"] = 0
        with self.assertRaisesRegex(RuntimeError, "settings"):
            GATE.validate_pair(self.reports, self.traces)

    def test_tick_zero_cannot_pad_360_simulation_updates(self):
        self.traces["guest"] = {tick: "0123456789abcdef" for tick in range(359)}
        self.traces["guest"][480] = "0123456789abcdef"
        with self.assertRaisesRegex(RuntimeError, "360"):
            GATE.validate_pair(self.reports, self.traces)

    def test_duplicate_tick_records_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "trace.jsonl"
            path.write_text('{"tick":1,"digest":"x"}\n' * 2)
            with self.assertRaisesRegex(RuntimeError, "Duplicate"):
                GATE.load_trace(path)


if __name__ == "__main__":
    unittest.main()
