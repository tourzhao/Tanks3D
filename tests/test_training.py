import unittest
import tempfile
import json
from contextlib import closing
from pathlib import Path
from unittest.mock import Mock, patch
import numpy as np
import torch
import gymnasium as gym
from gymnasium.utils.env_checker import check_env
from stable_baselines3 import PPO
from training.env import TanksEnv, ROOT
from training.bot import Defender
from training.hybrid import HybridPolicy, guard_fire
from training.hybrid_env import HybridTrainingEnv
from training.rewards import DeathPenalty
from training.tutor import (
    TEACHER_NAMES,
    Navigator,
    MovingNavigator,
    TerrainTutor,
    CurriculumTutor,
    make_teacher,
    terrain_fingerprint,
)
from training.policy import (
    TankFeatures,
    TankLocalFeatures,
    TankTacticalFeatures,
    TankWideFeatures,
    TankContextFeatures,
    TankProjectileFeatures,
    add_context_weights,
    add_projectile_weights,
    copy_policy_weights,
    widen_policy_weights,
)
from training.rehearsal import Rehearsal, unsafe_fire_labels, unsafe_fire_loss
from training.distill import actor_parameters, distillation_loss
from stable_baselines3.common.vec_env import DummyVecEnv
from stable_baselines3.common.callbacks import BaseCallback
from stable_baselines3.common.monitor import Monitor


class TrainingTests(unittest.TestCase):
    def test_death_penalty_preserves_native_game_and_observations(self):
        deaths_seen = 0
        with DeathPenalty(TanksEnv(), 5) as shaped, TanksEnv() as native:
            rng = np.random.default_rng(10401)
            for seed, stage in ((10401, 1), (10402, 7), (10403, 32)):
                obs, info = shaped.reset(seed=seed, options={"stage": stage})
                expected, raw_info = native.reset(seed=seed, options={"stage": stage})
                self.assertEqual(info, raw_info)
                previous_deaths = info["deaths"]
                raw_return = shaped_return = 0
                while True:
                    for key in obs:
                        np.testing.assert_array_equal(obs[key], expected[key])
                    action = int(rng.integers(10))
                    obs, reward, term, trunc, info = shaped.step(action)
                    expected, raw, t2, t3, raw_info = native.step(action)
                    delta = info["deaths"] - previous_deaths
                    previous_deaths = info["deaths"]
                    deaths_seen += delta
                    self.assertEqual(reward, raw - 5 * delta)
                    self.assertEqual((term, trunc), (t2, t3))
                    self.assertEqual(shaped.unwrapped.digest(), native.digest())
                    self.assertEqual(
                        info.pop("reward_terms"),
                        {"native": raw, "extra_death": -5 * delta, "shaped": reward},
                    )
                    raw_return += raw
                    shaped_return += reward
                    if term or trunc:
                        self.assertEqual(
                            info.pop("episode_reward_terms"),
                            {
                                "native": raw_return,
                                "extra_death": -5 * info["deaths"],
                                "shaped": shaped_return,
                            },
                        )
                        self.assertEqual(info, raw_info)
                        break
                    self.assertEqual(info, raw_info)
        self.assertGreater(deaths_seen, 0)

    def test_death_penalty_deltas_scaling_and_episode_boundaries(self):
        class Scenario(gym.Env):
            action_space = gym.spaces.Discrete(10)
            observation_space = gym.spaces.Box(0, 1, shape=(1,), dtype=np.float32)

            def __init__(self, timeout):
                self.timeout = timeout

            def reset(self, *, seed=None, options=None):
                super().reset(seed=seed)
                self.steps = 0
                return np.zeros(1, np.float32), {"deaths": 2, "reward": 0}

            def step(self, action):
                self.steps += 1
                rewards = [1, -4, 0, 0 if self.timeout else -33]
                deaths = [2, 3, 3, 3 if self.timeout else 4]
                ended = self.steps == 4
                reward = rewards[self.steps - 1]
                return (
                    np.zeros(1, np.float32),
                    reward,
                    ended and not self.timeout,
                    ended and self.timeout,
                    {"deaths": deaths[self.steps - 1], "reward": reward},
                )

        for penalty in (0, 5):
            for timeout in (False, True):
                with self.subTest(penalty=penalty, timeout=timeout):
                    shaped = DeathPenalty(Scenario(timeout), penalty)
                    with Monitor(
                        gym.wrappers.TransformReward(shaped, lambda r: r * 0.1)
                    ) as env:
                        # Repeated reset must not count prior-episode deaths.
                        for _ in range(2):
                            env.reset(seed=11451)
                            values = []
                            for _ in range(4):
                                _, reward, term, trunc, info = env.step(0)
                                values.append(reward)
                            expected = [
                                0.1,
                                (-4 - penalty) * 0.1,
                                0,
                                0 if timeout else (-33 - penalty) * 0.1,
                            ]
                            np.testing.assert_allclose(values, expected)
                            self.assertEqual((term, trunc), (not timeout, timeout))
                            self.assertAlmostEqual(info["episode"]["r"], sum(expected))
                            total = info["episode_reward_terms"]
                            self.assertEqual(
                                total["extra_death"], -(1 if timeout else 2) * penalty
                            )
                            self.assertEqual(total["native"], -3 if timeout else -36)
                            self.assertAlmostEqual(total["shaped"] * 0.1, sum(expected))
                            self.assertEqual(info["reward"], 0 if timeout else -33)

    def test_death_penalty_validation_reset_and_close(self):
        with TanksEnv(max_seconds=0.1) as native:
            for value in (-1, float("nan"), float("inf")):
                with self.assertRaises(ValueError):
                    DeathPenalty(native, value)
            wrapper = DeathPenalty(native, 5)
            with self.assertRaises(RuntimeError):
                wrapper.step(0)
            wrapper.reset(seed=11452)
            digest = native.digest()
            with self.assertRaises(ValueError):
                wrapper.step(10)
            self.assertEqual(native.digest(), digest)
            self.assertEqual(wrapper.totals, {"native": 0, "extra_death": 0, "shaped": 0})
            wrapper.step(0)
            _, _, _, truncated, _ = wrapper.step(0)
            self.assertTrue(truncated)
            with self.assertRaises(RuntimeError):
                wrapper.step(0)
            wrapper.reset(seed=11452)
            self.assertEqual(wrapper.totals, {"native": 0, "extra_death": 0, "shaped": 0})
            wrapper.close()
            with self.assertRaises(RuntimeError):
                wrapper.step(0)

    def test_hybrid_training_native_parity_and_reset(self):
        proposal = Mock()
        runtime = HybridPolicy(proposal)
        branches = set()
        with HybridTrainingEnv(TanksEnv()) as wrapped, TanksEnv() as native:
            rng = np.random.default_rng(10401)
            for seed, stage in ((10401, 1), (10402, 7), (10403, 32)):
                obs, info = wrapped.reset(seed=seed, options={"stage": stage})
                reference, expected_info = native.reset(seed=seed, options={"stage": stage})
                runtime.reset()
                self.assertEqual(info, expected_info)
                while True:
                    for key in obs:
                        np.testing.assert_array_equal(obs[key], reference[key])
                    action = int(rng.integers(10))
                    proposal.predict.return_value = (action, None)
                    actual = runtime.predict(reference)
                    # A caller retaining or modifying returned observations must
                    # not change the wrapper's next arbitration decision.
                    for value in obs.values():
                        value.fill(0)
                    obs, reward, terminated, truncated, info = wrapped.step(action)
                    reference, expected_reward, t2, t3, expected_info = native.step(actual)
                    self.assertEqual((reward, terminated, truncated), (expected_reward, t2, t3))
                    self.assertEqual(wrapped.unwrapped.digest(), native.digest())
                    event = info.pop("hybrid_action")
                    branches.add(event["defender"])
                    self.assertEqual((event["proposed"], event["executed"]), (action, actual))
                    self.assertEqual(wrapped.controller.statistics(), runtime.statistics())
                    if terminated or truncated:
                        terminal = info.pop("hybrid_training")
                        self.assertEqual(terminal["decisions"], runtime.counts["decisions"])
                        self.assertEqual(info, expected_info)
                        with self.assertRaises(RuntimeError):
                            wrapped.step(0)
                        break
                    self.assertEqual(info, expected_info)
        self.assertEqual(branches, {False, True})

    def test_hybrid_training_invalid_proposal_is_atomic(self):
        wrapped = HybridTrainingEnv(TanksEnv(max_seconds=0.1))
        try:
            with self.assertRaises(RuntimeError):
                wrapped.step(0)
            wrapped.reset(seed=10404)
            stats, digest = wrapped.controller.statistics(), wrapped.unwrapped.digest()
            for invalid in (-1, 10, 3.5, [3], np.array([3])):
                with self.assertRaises(ValueError):
                    wrapped.step(invalid)
                self.assertEqual(stats, wrapped.controller.statistics())
                self.assertEqual(digest, wrapped.unwrapped.digest())
            wrapped.step(0)
            _, _, _, truncated, info = wrapped.step(0)
            self.assertTrue(truncated)
            self.assertEqual(info["hybrid_training"]["decisions"], 2)
            wrapped.reset(seed=10404)
            self.assertTrue(all(v == 0 for v in wrapped.controller.statistics().values()))
        finally:
            wrapped.close()
        with self.assertRaises(RuntimeError):
            wrapped.step(0)
        with self.assertRaises(RuntimeError):
            wrapped.reset(seed=10404)

    def test_hybrid_training_ppo_retains_proposal_likelihoods(self):
        test = self

        class Scenario(gym.Env):
            def __init__(self, threat):
                self.observation = test.hybrid_observation()
                self.observation["state"][:4] = [13 / 26, 19 / 26, 0, 1]
                if threat:
                    self.observation["state"][16:19] = [1, 13 / 26, 20 / 26]
                self.action_space = gym.spaces.Discrete(10)
                self.observation_space = gym.spaces.Dict(
                    {
                        k: gym.spaces.Box(-16, 16, shape=v.shape, dtype=np.float32)
                        for k, v in self.observation.items()
                    }
                )

            def reset(self, *, seed=None, options=None):
                super().reset(seed=seed)
                self.steps = 0
                return {k: v.copy() for k, v in self.observation.items()}, {}

            def step(self, action):
                self.steps += 1
                return (
                    {k: v.copy() for k, v in self.observation.items()},
                    float(action == 4),
                    self.steps == 8,
                    False,
                    {},
                )

        class InspectRollout(BaseCallback):
            def __init__(self):
                super().__init__()
                self.actions = []

            def _on_step(self):
                self.actions.append([i["hybrid_action"] for i in self.locals["infos"]])
                return True

            def _on_rollout_end(self):
                buffer = self.model.rollout_buffer
                proposed = np.array([[i["proposed"] for i in step] for step in self.actions])
                np.testing.assert_array_equal(buffer.actions[:, :, 0], proposed)
                observation = {
                    k: torch.as_tensor(v.reshape((-1, *v.shape[2:])))
                    for k, v in buffer.observations.items()
                }
                with torch.no_grad():
                    _, logs, _ = self.model.policy.evaluate_actions(
                        observation, torch.as_tensor(proposed.reshape(-1))
                    )
                np.testing.assert_allclose(
                    logs.numpy(), buffer.log_probs.reshape(-1), rtol=1e-5, atol=1e-6
                )
                test.assertTrue(any(step[0]["fire_suppressed"] for step in self.actions))
                test.assertTrue(all(step[1]["defender"] for step in self.actions))
                test.assertTrue(all(step[1]["executed"] == 0 for step in self.actions))
                test.assertTrue(any(step[1]["proposed"] != 0 for step in self.actions))

        torch.set_num_threads(1)
        with closing(
            DummyVecEnv(
                [
                    lambda: HybridTrainingEnv(Scenario(False), "base-guard"),
                    lambda: HybridTrainingEnv(Scenario(True), "defense"),
                ]
            )
        ) as env:
            model = PPO(
                "MultiInputPolicy",
                env,
                seed=10405,
                n_steps=32,
                batch_size=16,
                n_epochs=1,
                policy_kwargs={
                    "features_extractor_class": TankFeatures,
                    "net_arch": {"pi": [64], "vf": [64]},
                    "normalize_images": False,
                },
                device="cpu",
            )
            with torch.no_grad():
                model.policy.action_net.bias[5] = 3
            before = {k: v.clone() for k, v in model.policy.state_dict().items()}
            with patch.object(env.envs[1].controller.defender, "predict", return_value=1):
                model.learn(64, callback=InspectRollout())
            after = model.policy.state_dict()
            self.assertTrue(all(torch.isfinite(v).all() for v in after.values()))
            self.assertTrue(any(not torch.equal(before[k], v) for k, v in after.items()))

    @staticmethod
    def hybrid_observation():
        obs = {"map": np.zeros((14, 26, 26), np.float32), "state": np.zeros(256, np.float32)}
        obs["state"][:4] = [9 / 26, 21 / 26, 0, -1]
        obs["state"][8] = obs["state"][246] = 1
        return obs

    def test_hybrid_fire_guard_keeps_motion_and_stopped_heading(self):
        cases = (
            (13, 19, 0, 1, 5, 4),  # South toward the base.
            (13, 19, 0, 1, 1, 0),  # Stopped south-facing shot.
            (19, 25, -1, 0, 7, 6),
            (19, 25, -1, 0, 1, 0),
            (7, 25, 1, 0, 9, 8),
            (7, 25, 1, 0, 1, 0),
            (13, 19, 0, -1, 3, 3),
            (13, 19, 0, -1, 1, 1),
            (19, 25, 1, 0, 9, 9),
            (7, 25, -1, 0, 7, 7),
            (10.49, 19, 0, 1, 5, 5),
            (10.51, 19, 0, 1, 5, 4),
            (15.49, 19, 0, 1, 5, 4),
            (15.51, 19, 0, 1, 5, 5),
            (19, 22.49, -1, 0, 7, 7),
            (19, 22.51, -1, 0, 7, 6),
        )
        for x, z, dx, dz, action, expected in cases:
            with self.subTest(x=x, z=z, action=action, heading=(dx, dz)):
                obs = self.hybrid_observation()
                obs["state"][:4] = [x / 26, z / 26, dx, dz]
                original = {key: value.copy() for key, value in obs.items()}
                self.assertEqual(guard_fire(obs, action), expected)
                self.assertEqual(expected // 2, action // 2)
                for nofire in range(0, 10, 2):
                    self.assertEqual(guard_fire(obs, nofire), nofire)
                for key in obs:
                    np.testing.assert_array_equal(obs[key], original[key])

    def test_hybrid_guard_only_never_calls_defender(self):
        model = Mock()
        model.predict.return_value = (5, None)
        hybrid = HybridPolicy(model, "base-guard")
        obs = self.hybrid_observation()
        obs["state"][:2] = [13 / 26, 19 / 26]
        obs["state"][16:19] = [1, 13 / 26, 20 / 26]
        with patch.object(hybrid.defender, "predict") as teacher:
            self.assertEqual(hybrid.predict(obs, deterministic=False), 4)
            self.assertFalse(teacher.called)
        self.assertIs(model.predict.call_args.args[0], obs)
        self.assertFalse(model.predict.call_args.kwargs["deterministic"])
        stats = hybrid.statistics()
        self.assertEqual((stats["neural_calls"], stats["defender_calls"]), (1, 0))
        self.assertEqual((stats["fire_suppressions"], stats["rule_decisions"]), (1, 1))
        with self.assertRaises(ValueError):
            HybridPolicy(model, "unknown")

    def test_hybrid_defense_hysteresis_and_reset(self):
        model = Mock()
        model.predict.return_value = (3, None)
        hybrid = HybridPolicy(model)
        obs = self.hybrid_observation()
        obs["state"][16:19] = [1, 13 / 26, 14 / 26]  # Distance 11: outside entry.
        with patch.object(hybrid.defender, "predict", return_value=9) as teacher:
            self.assertEqual(hybrid.predict(obs), 3)
            obs["state"][18] = 16 / 26  # Distance 9: enter.
            self.assertEqual(hybrid.predict(obs), 9)
            obs["state"][18] = 14 / 26  # Distance 11: keep defending.
            self.assertEqual(hybrid.predict(obs), 9)
            hybrid.defender.route = [(3, 3)]
            obs["state"][18] = 12 / 26  # Distance 13: release.
            self.assertEqual(hybrid.predict(obs), 3)
            self.assertEqual(hybrid.defender.route, [])
            self.assertEqual((model.predict.call_count, teacher.call_count), (2, 2))
        self.assertEqual(hybrid.statistics()["defense_entries"], 1)
        self.assertEqual(hybrid.statistics()["rule_fraction"], 0.5)
        hybrid.defender.route = [(7, 7)]
        hybrid.reset()
        self.assertFalse(hybrid.defending)
        self.assertEqual(hybrid.defender.route, [])
        self.assertTrue(all(value == 0 for value in hybrid.statistics().values()))

    def test_hybrid_ignores_unavailable_enemies_and_player(self):
        model = Mock()
        model.predict.return_value = (3, None)
        for unavailable in (
            "absent",
            "creating",
            "frozen",
            "player-inactive",
            "player-creating",
            "respawn",
        ):
            with self.subTest(unavailable=unavailable):
                obs = self.hybrid_observation()
                obs["state"][16:19] = [1, 13 / 26, 20 / 26]
                index = {
                    "absent": 16,
                    "creating": 25,
                    "frozen": 26,
                    "player-inactive": 8,
                    "player-creating": 12,
                    "respawn": 13,
                }[unavailable]
                obs["state"][index] = 0 if unavailable in ("absent", "player-inactive") else 1
                hybrid = HybridPolicy(model)
                with patch.object(hybrid.defender, "predict") as teacher:
                    self.assertEqual(hybrid.predict(obs), 3)
                    self.assertFalse(teacher.called)

    def test_hybrid_filters_teacher_copy_and_counts_interventions_once(self):
        model = Mock()
        hybrid = HybridPolicy(model)
        obs = self.hybrid_observation()
        obs["state"][:4] = [13 / 26, 19 / 26, 0, 1]
        enemies = obs["state"][16:64].reshape(4, 12)
        enemies[:3, :3] = [[1, 13 / 26, 20 / 26], [1, 13 / 26, 24 / 26], [1, 9 / 26, 22 / 26]]
        enemies[1, 9] = enemies[2, 10] = 1
        original = {key: value.copy() for key, value in obs.items()}
        with patch.object(hybrid.defender, "predict", return_value=1) as teacher:
            self.assertEqual(hybrid.predict(obs), 0)
            taught = teacher.call_args.args[0]
            np.testing.assert_array_equal(
                taught["state"][16:64].reshape(4, 12)[:, 0], [1, 0, 0, 0]
            )
            self.assertIs(taught["map"], obs["map"])
        self.assertFalse(model.predict.called)
        for key in obs:
            np.testing.assert_array_equal(obs[key], original[key])
        stats = hybrid.statistics()
        self.assertEqual(
            (stats["decisions"], stats["rule_decisions"], stats["fire_suppressions"]), (1, 1, 1)
        )
        self.assertEqual((stats["defender_calls"], stats["neural_calls"]), (1, 0))

    def test_hybrid_seeded_native_actions_replay(self):
        model = Mock()
        model.predict.return_value = (3, None)
        hybrid = HybridPolicy(model)
        with TanksEnv(max_seconds=20) as first, TanksEnv(max_seconds=20) as replay:
            obs, _ = first.reset(seed=480121, options={"stage": 7})
            replay.reset(seed=480121, options={"stage": 7})
            for _ in range(400):
                action = hybrid.predict(obs)
                obs, reward, term, trunc, info = first.step(action)
                other, other_reward, other_term, other_trunc, other_info = replay.step(action)
                self.assertEqual(
                    (reward, term, trunc, info),
                    (other_reward, other_term, other_trunc, other_info),
                )
                self.assertEqual(first.digest(), replay.digest())
                np.testing.assert_array_equal(obs["state"], other["state"])
                if term or trunc:
                    break
        stats = hybrid.statistics()
        self.assertEqual(stats["decisions"], stats["neural_calls"] + stats["defender_calls"])

    def test_gym_contract_and_seeded_episodes(self):
        with TanksEnv() as env:
            check_env(env, skip_render_check=True)

    def test_same_seed_actions_and_observation_ownership(self):
        with TanksEnv(max_seconds=10) as first, TanksEnv(max_seconds=10) as second:
            a, _ = first.reset(seed=915)
            original = a["state"].copy()
            second.reset(seed=915)
            for i in range(150):
                action = 3 if i < 110 else 9
                a, ar, at, ax, ai = first.step(action)
                b, br, bt, bx, bi = second.step(action)
                np.testing.assert_array_equal(a["map"], b["map"])
                np.testing.assert_array_equal(a["state"], b["state"])
                self.assertEqual((ar, at, ax, ai), (br, bt, bx, bi))
                self.assertEqual(first.digest(), second.digest())
            self.assertNotEqual(float(original[1]), float(a["state"][1]))
            a["state"][0] = 999
            b, *_ = first.step(3)
            self.assertLess(b["state"][0], 1)

    def test_time_limit_is_truncation(self):
        with TanksEnv(max_seconds=0.05) as env:
            env.reset(seed=33)
            _, _, terminated, truncated, info = env.step(0)
            self.assertFalse(terminated)
            self.assertTrue(truncated)
            self.assertEqual(info["ticks"], 3)
            with self.assertRaises(RuntimeError):
                env.step(0)
            env.reset(seed=33)
            with self.assertRaises(ValueError):
                env.step(10)

    def test_stage_clear_waits_for_live_grace_period(self):
        # Fixed integration episode, not a training demonstration. A final
        # kill must not prematurely end the production five-second grace.
        with TanksEnv() as env:
            obs, _ = env.reset(seed=100001)
            bot = Defender()
            transition_tick = None
            while True:
                obs, reward, terminated, truncated, info = env.step(bot.predict(obs))
                if obs["state"][247] and transition_tick is None:
                    transition_tick = info["ticks"]
                    self.assertFalse(terminated)
                    self.assertEqual(info["won"], 0)
                if terminated or truncated:
                    self.assertTrue(terminated)
                    self.assertFalse(truncated)
                    self.assertEqual(info["won"], 1)
                    self.assertGreaterEqual(reward, 50)
                    self.assertIsNotNone(transition_tick)
                    self.assertGreaterEqual(info["ticks"] - transition_tick, 298)
                    self.assertLessEqual(info["ticks"] - transition_tick, 303)
                    break

    def test_defender_and_original_maps(self):
        with TanksEnv() as env:
            bot = Defender()
            for stage in (1, 2, 10, 35):
                obs, _ = env.reset(seed=57, options={"stage": stage})
                self.assertTrue(env.observation_space.contains(obs))
                # Original headquarters remains occupied and enclosed.
                self.assertTrue((obs["map"][9, 24:26, 12:14] == 1).all())
                self.assertEqual(np.count_nonzero(obs["map"][8]), 8)
                bot.reset()
                for _ in range(30):
                    action = bot.predict(obs)
                    self.assertTrue(env.action_space.contains(action))
                    obs, _, term, trunc, _ = env.step(action)
                    if term or trunc:
                        break

    def test_closed_environment(self):
        env = TanksEnv()
        env.close()
        env.close()
        with self.assertRaises(RuntimeError):
            env.reset(seed=1)

    def test_tutor_respects_cover_and_even_integer_lanes(self):
        terrain = np.zeros((14, 26, 26), dtype=np.float32)
        self.assertTrue(Navigator.clear_shot(terrain, 9, 21, 9, 5, 1))
        terrain[4, 12:14, 8:10] = 1
        self.assertFalse(Navigator.clear_shot(terrain, 9, 21, 9, 5, 1))
        terrain[4] = 0
        terrain[0:4, 12:14, 8:10] = 1
        self.assertTrue(Navigator.clear_shot(terrain, 9, 21, 9, 5, 1))
        self.assertFalse(Navigator.clear_shot(terrain, 13, 10, 13, 23, 2))
        terrain[:] = 0
        terrain[4] = 1
        terrain[4, :, 11:13] = 0  # Only the even x=12 lane fits the tank.
        distance, _ = Navigator.navigation(terrain, (12, 25), False)
        self.assertEqual(distance[(12, 1)], 24)
        self.assertTrue(all(x == 12 for x, _ in distance))
        terrain[5, 10:12, 11:13] = 1
        self.assertNotIn((12, 1), Navigator.navigation(terrain, (12, 25), False)[0])
        self.assertIn((12, 1), Navigator.navigation(terrain, (12, 25), True)[0])

    def test_tutor_holds_a_clear_firing_lane_and_resets(self):
        obs = {
            "map": np.zeros((14, 26, 26), np.float32),
            "state": np.zeros(256, np.float32),
        }
        obs["state"][:4] = [9 / 26, 21 / 26, 0, -1]
        obs["state"][16:19] = [1, 9 / 26, 5 / 26]
        tutor = Navigator()
        self.assertEqual(tutor.predict(obs), 1)
        # A mismatched heading must turn before a stopped shot is valid.
        obs["state"][2:4] = [1, 0]
        self.assertEqual(tutor.predict(obs), 3)
        tutor.route = [(5, 5)]
        tutor.reset()
        self.assertEqual((tutor.calls, tutor.route), (0, []))
        self.assertIsInstance(make_teacher("defender"), Defender)
        with self.assertRaises(ValueError):
            make_teacher("missing")

    def test_moving_tutor_preserves_a_valid_firing_heading(self):
        obs = {"map": np.zeros((14, 26, 26), np.float32), "state": np.zeros(256, np.float32)}
        obs["state"][:4] = [9 / 26, 21 / 26, 0, -1]
        obs["state"][16:19] = [1, 9 / 26, 5 / 26]
        self.assertEqual(Navigator().predict(obs), 1)
        self.assertEqual(MovingNavigator().predict(obs), 3)

    def test_terrain_tutor_obstruction_footprint_and_boat(self):
        obs = {"map": np.zeros((14, 26, 26), np.float32), "state": np.zeros(256)}
        obs["state"][:4] = [9 / 26, 11 / 26, 0, -1]
        obs["map"][4, 9, 8] = 1
        self.assertTrue(TerrainTutor.blocked_ahead(obs, 1))
        self.assertFalse(TerrainTutor.blocked_ahead(obs, 4))
        obs["map"][4] = 0
        obs["map"][:4, 9, 8] = 1  # Destructible cover can be cleared by shooting.
        self.assertFalse(TerrainTutor.blocked_ahead(obs, 1))
        obs["map"][5, 9, 8] = 1
        self.assertTrue(TerrainTutor.blocked_ahead(obs, 1))
        obs["state"][9] = 1
        self.assertFalse(TerrainTutor.blocked_ahead(obs, 1))
        obs["state"][:2] = [1 / 26, 1 / 26]
        self.assertTrue(TerrainTutor.blocked_ahead(obs, 1))
        self.assertTrue(TerrainTutor.blocked_ahead(obs, 3))
        obs["map"][:] = 0
        obs["state"][:4] = [9.875 / 26, 11 / 26, 1, 0]
        obs["map"][4, 10:12, 11] = 1
        self.assertFalse(TerrainTutor.blocked_ahead(obs, 4))  # Exact contact.
        obs["state"][0] = 9.885 / 26
        self.assertTrue(TerrainTutor.blocked_ahead(obs, 4))

    def test_terrain_tutor_changes_only_blocked_nonice_moves(self):
        obs = {"map": np.zeros((14, 26, 26), np.float32), "state": np.zeros(256, np.float32)}
        obs["state"][:4] = [9 / 26, 11 / 26, 0, -1]
        tutor = TerrainTutor()
        with patch.object(tutor.defender, "predict", return_value=3) as original:
            with patch.object(tutor.navigator, "predict", return_value=9) as alternate:
                self.assertEqual(tutor.predict(obs), 3)
                obs["map"][4, 9, 8] = 1
                self.assertEqual(tutor.predict(obs), 9)
                obs["state"][14] = 1
                self.assertEqual(tutor.predict(obs), 3)
                self.assertEqual((original.call_count, alternate.call_count), (3, 3))
        tutor.defender.route = [(3, 3)]
        tutor.navigator.route = [(5, 5)]
        tutor.reset()
        self.assertEqual((tutor.defender.route, tutor.navigator.route), ([], []))

    def test_training_seeds_stay_outside_evaluation_range(self):
        with TanksEnv(seed_limit=60000) as env:
            env.reset(seed=42)
            for _ in range(100):
                _, info = env.reset()
                self.assertLess(info["episode_seed"], 60000)
        with self.assertRaises(ValueError):
            TanksEnv(seed_limit=0)

    def test_curriculum_selects_only_initial_terrain_and_resets(self):
        output = ROOT / "build/tests"
        output.mkdir(parents=True, exist_ok=True)
        with TanksEnv() as env, tempfile.TemporaryDirectory(dir=output) as directory:
            first, _ = env.reset(seed=831, options={"stage": 1})
            second, _ = env.reset(seed=832, options={"stage": 7})
            manifest = Path(directory) / "curriculum.json"
            manifest.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "maps": {
                            terrain_fingerprint(first): {"teacher": "defender"},
                            terrain_fingerprint(second): {"teacher": "moving-navigator"},
                        },
                    }
                )
            )
            tutor = make_teacher("terrain", manifest)
            self.assertIsInstance(tutor, CurriculumTutor)
            self.assertEqual(tutor.predict(first), Defender().predict(first))
            self.assertEqual(tutor.selected_name, "defender")
            tutor.predict(second)  # Altered terrain mid-episode must not switch teachers.
            self.assertEqual(tutor.selected_name, "defender")
            tutor.reset()
            self.assertEqual(tutor.predict(second), MovingNavigator().predict(second))
            self.assertEqual(tutor.selected_name, "moving-navigator")
            third, _ = env.reset(seed=833, options={"stage": 35})
            tutor.reset()
            self.assertEqual(tutor.predict(third), TerrainTutor().predict(third))
            self.assertEqual(tutor.selected_name, "terrain")
            manifest.write_text(
                json.dumps({"schema": 1, "maps": {"bad": {"teacher": "missing"}}})
            )
            with self.assertRaises(ValueError):
                make_teacher("terrain", manifest)

    def test_checkpoint_preserves_deterministic_and_sampled_actions(self):
        torch.set_num_threads(1)
        output = ROOT / "build/tests"
        output.mkdir(parents=True, exist_ok=True)
        with TanksEnv() as env, tempfile.TemporaryDirectory(dir=output) as directory:
            obs, _ = env.reset(seed=70000)
            for features in (
                TankFeatures,
                TankLocalFeatures,
                TankTacticalFeatures,
                TankWideFeatures,
                TankContextFeatures,
                TankProjectileFeatures,
            ):
                model = PPO(
                    "MultiInputPolicy",
                    env,
                    n_steps=8,
                    batch_size=8,
                    seed=12,
                    policy_kwargs={
                        "features_extractor_class": features,
                        "normalize_images": False,
                    },
                )
                checkpoint = f"{directory}/{features.__name__}"
                model.save(checkpoint)
                restored = PPO.load(checkpoint, device="cpu")
                for deterministic in (True, False):
                    torch.manual_seed(99)
                    expected = [
                        int(model.predict(obs, deterministic=deterministic)[0])
                        for _ in range(20)
                    ]
                    torch.manual_seed(99)
                    actual = [
                        int(restored.predict(obs, deterministic=deterministic)[0])
                        for _ in range(20)
                    ]
                    self.assertEqual(expected, actual)

    def test_separate_value_preserves_policy_and_isolates_value_gradients(self):
        torch.set_num_threads(1)
        with TanksEnv() as env:
            obs, _ = env.reset(seed=731)
            kwargs = {
                "features_extractor_class": TankLocalFeatures,
                "normalize_images": False,
            }
            shared = PPO(
                "MultiInputPolicy",
                env,
                n_steps=8,
                batch_size=8,
                policy_kwargs=kwargs,
                seed=33,
            )
            split = PPO(
                "MultiInputPolicy",
                env,
                n_steps=8,
                batch_size=8,
                policy_kwargs={**kwargs, "share_features_extractor": False},
                seed=44,
            )
            split.policy.load_state_dict(shared.policy.state_dict(), strict=True)
            tensor = split.policy.obs_to_tensor(obs)[0]
            expected = (
                shared.policy.get_distribution(tensor).distribution.probs.detach().clone()
            )
            actual = split.policy.get_distribution(tensor).distribution.probs
            torch.testing.assert_close(actual, expected, rtol=0, atol=0)
            optimizer = torch.optim.SGD(split.policy.parameters(), lr=0.1)
            optimizer.zero_grad()
            split.policy.predict_values(tensor).sum().backward()
            optimizer.step()
            after = split.policy.get_distribution(tensor).distribution.probs
            torch.testing.assert_close(after, expected, rtol=0, atol=0)
            tactical = PPO(
                "MultiInputPolicy",
                env,
                n_steps=8,
                batch_size=8,
                policy_kwargs={
                    **kwargs,
                    "features_extractor_class": TankTacticalFeatures,
                    "share_features_extractor": False,
                },
            )
            copy_policy_weights(shared.policy, tactical.policy)
            actual = tactical.policy.get_distribution(tensor).distribution.probs
            torch.testing.assert_close(actual, expected, rtol=0, atol=0)

    def test_widening_preserves_actor_value_and_trains_extra_capacity(self):
        torch.set_num_threads(1)
        with TanksEnv() as env:
            observations = []
            for stage in (1, 7, 21, 35):
                obs, _ = env.reset(seed=631 + stage, options={"stage": stage})
                observations.append(obs)
            inputs = {
                key: torch.as_tensor(np.stack([obs[key] for obs in observations]))
                for key in observations[0]
            }
            narrow = PPO(
                "MultiInputPolicy",
                env,
                seed=35,
                n_steps=8,
                batch_size=8,
                policy_kwargs={
                    "features_extractor_class": TankTacticalFeatures,
                    "net_arch": {"pi": [64], "vf": [64]},
                    "share_features_extractor": False,
                    "normalize_images": False,
                },
            )
            wide = PPO(
                "MultiInputPolicy",
                env,
                seed=57,
                n_steps=8,
                batch_size=8,
                policy_kwargs={
                    "features_extractor_class": TankWideFeatures,
                    "net_arch": {"pi": [128], "vf": [128]},
                    "share_features_extractor": False,
                    "normalize_images": False,
                },
            )
            widen_policy_weights(narrow.policy, wide.policy)
            with torch.no_grad():
                expected = narrow.policy.get_distribution(inputs).distribution.probs
                actual = wide.policy.get_distribution(inputs).distribution.probs
                torch.testing.assert_close(actual, expected, rtol=1e-5, atol=1e-7)
                torch.testing.assert_close(
                    wide.policy.predict_values(inputs),
                    narrow.policy.predict_values(inputs),
                    rtol=1e-5,
                    atol=1e-6,
                )
            optimizer = torch.optim.Adam(wide.policy.parameters(), lr=1e-3)
            for _ in range(2):
                optimizer.zero_grad()
                loss = (
                    -wide.policy.get_distribution(inputs)
                    .log_prob(torch.tensor([3, 6, 8, 9]))
                    .mean()
                )
                loss.backward()
                optimizer.step()
            extra = wide.policy.pi_features_extractor.map_net[0].weight.grad[8:]
            self.assertGreater(float(extra.abs().sum()), 0)
            self.assertTrue(
                all(p.grad is None for p in wide.policy.vf_features_extractor.parameters())
            )
            with self.assertRaises(ValueError):
                widen_policy_weights(wide.policy, narrow.policy)
            context = PPO(
                "MultiInputPolicy",
                env,
                seed=71,
                n_steps=8,
                batch_size=8,
                policy_kwargs={
                    "features_extractor_class": TankContextFeatures,
                    "net_arch": {"pi": [128], "vf": [128]},
                    "share_features_extractor": False,
                    "normalize_images": False,
                },
            )
            add_context_weights(wide.policy, context.policy)
            with torch.no_grad():
                torch.testing.assert_close(
                    context.policy.get_distribution(inputs).distribution.probs,
                    wide.policy.get_distribution(inputs).distribution.probs,
                    rtol=0,
                    atol=0,
                )
                torch.testing.assert_close(
                    context.policy.predict_values(inputs),
                    wide.policy.predict_values(inputs),
                    rtol=0,
                    atol=0,
                )
            optimizer = torch.optim.Adam(context.policy.parameters(), lr=1e-3)
            for _ in range(2):
                optimizer.zero_grad()
                loss = (
                    -context.policy.get_distribution(inputs)
                    .log_prob(torch.tensor([3, 6, 8, 9]))
                    .mean()
                )
                loss.backward()
                optimizer.step()
            for index in (4, 5):
                extra = context.policy.pi_features_extractor.map_net[index].conv_in.weight.grad
                self.assertGreater(float(extra.abs().sum()), 0)
            self.assertTrue(
                all(p.grad is None for p in context.policy.vf_features_extractor.parameters())
            )

    def test_projectile_features_respect_direction_units_and_ownership(self):
        state = torch.zeros(5, 256)
        state[:, :2] = 0.5
        state[:, 64:70] = torch.tensor(
            [
                [1, 10 / 26, 13 / 26, 6 / 30, 0, 0],
                [1, 13 / 26, 10 / 26, 0, 6 / 30, 0],
                [1, 10 / 26, 14 / 26, 6 / 30, 0, 0],
                [1, 10 / 26, 13 / 26, -6 / 30, 0, 0],
                [-1, 10 / 26, 13 / 26, 6 / 30, 0, 0],
            ]
        )
        cues = TankProjectileFeatures.flight_features(state).reshape(5, 24, 4)
        torch.testing.assert_close(
            cues[:, 0],
            torch.tensor(
                [
                    [0.5, 0, 1, 0.25],
                    [0.5, 0, 1, 0.25],
                    [0.5, 1 / 3, 0, 0.25],
                    [-0.5, 0, 1, -0.25],
                    [0, 0, 0, 0],
                ]
            ),
        )
        self.assertEqual(float(cues[:, 1:].abs().sum()), 0)
        self.assertTrue(bool(torch.isfinite(cues).all()))

    def test_projectile_migration_preserves_outputs_and_learns_cues(self):
        torch.set_num_threads(1)
        with TanksEnv() as env:
            obs, _ = env.reset(seed=971)
            inputs = {key: torch.as_tensor(value[None]) for key, value in obs.items()}
            inputs["state"][0, 64:70] = torch.tensor([1, 0.5, 0.5, 0.2, 0, 0])
            options = {
                "net_arch": {"pi": [128], "vf": [128]},
                "share_features_extractor": False,
                "normalize_images": False,
            }
            source, target = [
                PPO(
                    "MultiInputPolicy",
                    env,
                    seed=97,
                    n_steps=8,
                    batch_size=8,
                    policy_kwargs={**options, "features_extractor_class": encoder},
                )
                for encoder in (TankWideFeatures, TankProjectileFeatures)
            ]
            add_projectile_weights(source.policy, target.policy)
            with torch.no_grad():
                torch.testing.assert_close(
                    target.policy.get_distribution(inputs).distribution.probs,
                    source.policy.get_distribution(inputs).distribution.probs,
                    rtol=0,
                    atol=0,
                )
                torch.testing.assert_close(
                    target.policy.predict_values(inputs),
                    source.policy.predict_values(inputs),
                    rtol=0,
                    atol=0,
                )
            loss = -target.policy.get_distribution(inputs).log_prob(torch.tensor([3])).mean()
            loss.backward()
            self.assertGreater(
                float(target.policy.pi_features_extractor.projectiles.weight.grad.abs().sum()),
                0,
            )
            self.assertTrue(
                all(p.grad is None for p in target.policy.vf_features_extractor.parameters())
            )
            with self.assertRaises(ValueError):
                add_projectile_weights(target.policy, source.policy)

    def test_soft_distillation_learns_without_changing_teacher_or_critic(self):
        torch.set_num_threads(1)
        with TanksEnv() as env:
            rows = [
                env.reset(seed=991 + stage, options={"stage": stage})[0]
                for stage in (1, 7, 21, 35)
            ]
            inputs = {
                key: torch.as_tensor(np.stack([row[key] for row in rows])) for key in rows[0]
            }
            options = {
                "share_features_extractor": False,
                "normalize_images": False,
                "net_arch": {"pi": [64], "vf": [64]},
            }
            teacher, student = [
                PPO(
                    "MultiInputPolicy",
                    env,
                    seed=seed,
                    n_steps=8,
                    batch_size=8,
                    policy_kwargs={**options, "features_extractor_class": encoder},
                )
                for seed, encoder in ((97, TankWideFeatures), (131, TankTacticalFeatures))
            ]
            with torch.no_grad():
                teacher.policy.action_net.bias[3] += 1.5
            teacher_before = {
                key: value.clone() for key, value in teacher.policy.state_dict().items()
            }
            critic_before = [
                p.clone() for p in student.policy.vf_features_extractor.parameters()
            ]
            targets = teacher.policy.get_distribution(inputs).distribution.probs
            self.assertTrue(targets.requires_grad)
            parameters = actor_parameters(student.policy)
            self.assertEqual(sum(p.numel() for p in parameters), 201426)
            optimizer = torch.optim.Adam(parameters, lr=1e-3)
            initial = float(distillation_loss(student.policy, inputs, targets).detach())
            for _ in range(20):
                optimizer.zero_grad()
                loss = distillation_loss(student.policy, inputs, targets)
                loss.backward()
                optimizer.step()
            final = float(distillation_loss(student.policy, inputs, targets).detach())
            self.assertLess(final, initial / 2)
            self.assertTrue(all(p.grad is None for p in teacher.policy.parameters()))
            self.assertTrue(
                all(p.grad is None for p in student.policy.vf_features_extractor.parameters())
            )
            self.assertTrue(
                all(p.grad is None for p in student.policy.mlp_extractor.value_net.parameters())
            )
            self.assertTrue(all(p.grad is None for p in student.policy.value_net.parameters()))
            for key, value in teacher.policy.state_dict().items():
                torch.testing.assert_close(value, teacher_before[key], rtol=0, atol=0)
            for before, after in zip(
                critic_before, student.policy.vf_features_extractor.parameters()
            ):
                torch.testing.assert_close(before, after, rtol=0, atol=0)
            student.policy.share_features_extractor = True
            with self.assertRaises(ValueError):
                actor_parameters(student.policy)

    def test_rehearsal_labels_match_collected_states_across_resets(self):
        torch.set_num_threads(1)
        output = ROOT / "build/tests"
        output.mkdir(parents=True, exist_ok=True)
        for teacher_name, use_curriculum in [
            *((name, False) for name in TEACHER_NAMES),
            ("terrain", True),
        ]:
            with tempfile.TemporaryDirectory(dir=output) as directory:
                from pathlib import Path

                directory = Path(directory)
                env = DummyVecEnv([lambda: TanksEnv(max_seconds=0.2) for _ in range(2)])
                try:
                    obs = env.reset()
                    teacher_config = None
                    if use_curriculum:
                        teacher_config = directory / "teacher-config.json"
                        teacher_config.write_text(
                            json.dumps(
                                {
                                    "schema": 1,
                                    "maps": {
                                        terrain_fingerprint(
                                            {key: value[0] for key, value in obs.items()}
                                        ): {"teacher": "moving-navigator"}
                                    },
                                }
                            )
                        )
                    np.savez_compressed(
                        directory / "demos.npz",
                        map=np.rint(obs["map"] * 255).astype(np.uint8),
                        state=obs["state"],
                        action=np.array([3, 3], dtype=np.int64),
                    )
                    model = PPO(
                        "MultiInputPolicy",
                        env,
                        n_steps=16,
                        batch_size=16,
                        policy_kwargs={
                            "features_extractor_class": TankLocalFeatures,
                            "normalize_images": False,
                        },
                        seed=25,
                    )
                    callback = Rehearsal(
                        directory,
                        directory / "demos.npz",
                        batches=1,
                        safety_weight=2,
                        teacher=teacher_name,
                        teacher_config=teacher_config,
                    )
                    model.learn(64, callback=callback)
                    teachers = [make_teacher(teacher_name, teacher_config) for _ in range(2)]
                    count = 0
                    for shard in callback.memory:
                        for i, label in enumerate(shard["action"]):
                            observation = {
                                "map": shard["map"][i].astype(np.float32) / 255,
                                "state": shard["state"][i],
                            }
                            teacher = teachers[i % 2]
                            if observation["state"][244] == 0:
                                teacher.reset()
                            self.assertEqual(teacher.predict(observation), label)
                            count += 1
                    self.assertEqual(count, 64)
                    self.assertEqual(len(callback.history), 2)
                    self.assertTrue((directory / "last-corrections.npz").is_file())
                finally:
                    env.close()

    def test_training_safety_labels_and_gradient(self):
        # Exercise actual firing headings, including stop/keep-facing, near
        # the base and around each conservative teacher-corridor boundary.
        positions = [(13, 20), (9, 25), (17, 25), (3, 3), (10.4, 22.4), (15.6, 22.6)]
        headings = [(0, -1), (0, 1), (-1, 0), (1, 0)]
        state = torch.zeros((len(positions) * len(headings), 256))
        for i, (x, z) in enumerate(positions):
            for j, (dx, dz) in enumerate(headings):
                state[i * 4 + j, :4] = torch.tensor([x / 26, z / 26, dx, dz])
        labels = unsafe_fire_labels(state)
        for i, row in enumerate(state):
            x, z = (row[:2] * 26).tolist()
            current = 1 + headings.index(tuple(row[2:4].tolist()))
            for direction in range(5):
                expected = not Defender.safe_fire(x, z, direction or current)
                self.assertEqual(bool(labels[i, 2 * direction + 1]), expected)
                self.assertFalse(labels[i, 2 * direction])
        logits = torch.zeros((len(state), 10), requires_grad=True)
        before = (logits.softmax(-1) * labels).sum(-1).mean().item()
        loss = unsafe_fire_loss(logits.softmax(-1), state)
        loss.backward()
        after = ((logits - logits.grad).softmax(-1) * labels).sum(-1).mean().item()
        self.assertLess(after, before)
        safe = state[12:16]
        self.assertEqual(unsafe_fire_loss(torch.full((4, 10), 0.1), safe).item(), 0)

    def test_stochastic_dagger_uses_collection_seed_after_checkpoint_load(self):
        from training.imitate import main as collect

        torch.set_num_threads(1)
        output = ROOT / "build/tests"
        output.mkdir(parents=True, exist_ok=True)
        with TanksEnv() as env, tempfile.TemporaryDirectory(dir=output) as directory:
            directory = Path(directory)
            source = directory / "source.zip"
            model = PPO(
                "MultiInputPolicy",
                env,
                seed=234,
                n_steps=8,
                batch_size=8,
                policy_kwargs={
                    "features_extractor_class": TankFeatures,
                    "normalize_images": False,
                },
            )
            model.save(source)
            model = PPO.load(source, device="cpu")
            torch.manual_seed(123)
            rng = np.random.default_rng(123)
            obs, _ = env.reset(seed=int(rng.integers(2000, 60000)), options={"stage": 7})
            teacher = Defender()
            expected, labels = [], []
            for _ in range(32):
                label = teacher.predict(obs)
                labels.append(label)
                action = (
                    int(model.predict(obs, deterministic=False)[0])
                    if rng.random() < 0.75
                    else label
                )
                expected.append(action)
                obs, _, _, _, _ = env.step(action)
            executed = []
            original_step = TanksEnv.step

            def record_step(instance, action):
                executed.append(int(action))
                return original_step(instance, action)

            destination = directory / "data"
            arguments = [
                "training.imitate",
                "--collect-only",
                "--dagger",
                "--dagger-stochastic",
                "--model",
                str(source),
                "--seed",
                "123",
                "--samples",
                "32",
                "--stages",
                "7",
                "--output",
                str(destination),
            ]
            with patch("sys.argv", arguments), patch.object(TanksEnv, "step", record_step):
                collect()
            self.assertEqual(executed, expected)
            with np.load(destination / "demonstrations.npz") as data:
                np.testing.assert_array_equal(data["action"], labels)
            config = json.loads((destination / "config.json").read_text())
            self.assertEqual(config["action_rng_seed"], 123)

    def test_collect_only_uses_requested_original_stage(self):
        import json
        from training.imitate import main as collect

        torch.set_num_threads(1)
        output = ROOT / "build/tests"
        output.mkdir(parents=True, exist_ok=True)
        for teacher_name in TEACHER_NAMES:
            with tempfile.TemporaryDirectory(dir=output) as directory:
                destination = Path(directory) / "demos"
                arguments = [
                    "training.imitate",
                    "--collect-only",
                    "--teacher",
                    teacher_name,
                    "--output",
                    str(destination),
                    "--samples",
                    "4",
                    "--stages",
                    "7",
                    "10",
                ]
                with patch("sys.argv", arguments):
                    collect()
                config = json.loads((destination / "config.json").read_text())
                self.assertEqual(config["episode_stages"], [7])
                self.assertEqual(config["teacher"], teacher_name)
                self.assertIn("tutor.py", config["teacher_source_sha256"])
                self.assertFalse((destination / "model.zip").exists())
                with np.load(destination / "demonstrations.npz") as data, TanksEnv() as env:
                    obs, _ = env.reset(seed=config["episode_seeds"][0], options={"stage": 7})
                    teacher = make_teacher(teacher_name)
                    for i in range(4):
                        np.testing.assert_array_equal(data["state"][i], obs["state"])
                        np.testing.assert_array_equal(
                            data["map"][i], np.rint(obs["map"] * 255).astype(np.uint8)
                        )
                        self.assertEqual(int(data["action"][i]), teacher.predict(obs))
                        obs, *_ = env.step(int(data["action"][i]))


if __name__ == "__main__":
    unittest.main()
