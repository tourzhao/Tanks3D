"""The app's native AI must agree with the selected observation-only rules."""
import ctypes as ct
import unittest

import numpy as np

from training.env import ROOT
from training.coop_env import CoopEnv, player_view
from training.hybrid import guard_fire
from training.rule_policy import TacticalDefender


class NativeAiPlayerTests(unittest.TestCase):
    def compare_episode(self, stage, seed, seconds):
        with CoopEnv(max_seconds=seconds, native_path=ROOT / "build/tests/ai_player_probe.dylib") as env:
            lib = env.lib
            floats = np.ctypeslib.ndpointer(dtype=np.float32, ndim=1, flags="C_CONTIGUOUS")
            lib.t3rule_create.argtypes, lib.t3rule_create.restype = [], ct.c_void_p
            lib.t3rule_destroy.argtypes = [ct.c_void_p]
            lib.t3rule_predict.argtypes = [ct.c_void_p, ct.c_void_p, ct.c_int]
            lib.t3rule_observe.argtypes = [ct.c_void_p, ct.c_int, floats, floats]
            rules = [lib.t3rule_create(), lib.t3rule_create()]
            reference = [TacticalDefender(), TacticalDefender()]
            terrain, state = np.empty(10 * 676, np.float32), np.empty(340, np.float32)
            fields = [*range(64), *range(256, 272), 274, 275, 332, 333]
            obs, _ = env.reset(seed=seed, options={"stage": stage})
            count = 0
            try:
                while True:
                    actions = []
                    for slot in range(2):
                        view = player_view(obs, slot)
                        if count % 10 == 0:
                            before = env.digest()
                            lib.t3rule_observe(env.handle, slot, terrain, state)
                            np.testing.assert_array_equal(terrain, view["map"][:10].ravel())
                            np.testing.assert_array_equal(state[fields], view["state"][fields])
                            self.assertEqual(before, env.digest(), "Read-only AI observation changed native state")
                        expected = reference[slot].predict(view) if view["state"][332] > .5 else 0
                        expected = guard_fire(view, expected)
                        actual = lib.t3rule_predict(rules[slot], env.handle, slot)
                        self.assertEqual(actual, expected,
                            f"stage={stage} seed={seed} decision={count} slot={slot} "
                            f"xy={view['state'][:2] * 26} reason={reference[slot].last_reason}")
                        actions.append(actual)
                    obs, _, term, trunc, info = env.step(actions)
                    count += 1
                    if term or trunc:
                        return count, info, env.digest()
            finally:
                for rule in rules:
                    lib.t3rule_destroy(rule)

    def test_native_observation_and_policy_on_every_original_map(self):
        decisions = 0
        for stage in range(1, 36):
            with self.subTest(stage=stage):
                count, _, _ = self.compare_episode(stage, 3600000 + stage, 12)
                decisions += count
        self.assertEqual(decisions, 8400)

    def test_known_complete_episodes_preserve_reference_outcomes(self):
        cases = ((5, 3000004, "b1c2dddee2c73d49"),
                 (21, 3000020, "3d32d284c1ee3f4d"),
                 (33, 3000032, "517e6792024b6781"))
        for stage, seed, digest in cases:
            with self.subTest(stage=stage):
                _, _, actual = self.compare_episode(stage, seed, 120)
                self.assertEqual(actual, digest)


if __name__ == "__main__":
    unittest.main()
