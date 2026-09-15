import argparse
from contextlib import closing
from pathlib import Path
import tempfile
import unittest
import json
import subprocess
import sys

import gymnasium as gym
from gymnasium.utils.env_checker import check_env
import numpy as np
import torch
from stable_baselines3 import PPO

from training.env import ROOT, TanksEnv
from training.policy import TankTacticalFeatures
from training.coop_env import CoopEnv, player_view
from training.coop_policy import CoopActor, TeamCritic, save_actor, load_actor, tensor_observation
from training.coop_train import actor_objective, team_gae, execute_actions, update, initialize_models, propose_actions
from training.coop_partners import EpisodePartners, StageDeck
from training.coop_policy import actor_hash
from training.coop_control import CoopController
from training.coop_evaluate import MovementObserver


class CoopTrainingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        torch.set_num_threads(1)

    def test_gym_contract_and_slot_observations(self):
        with CoopEnv(max_seconds=1) as env:
            check_env(env, skip_render_check=True)
            obs, info = env.reset(seed=144001)
            self.assertEqual(obs["state"].shape, (2, 384))
            np.testing.assert_array_equal(obs["state"][0, 256:272], obs["state"][1, :16])
            np.testing.assert_array_equal(obs["state"][1, 256:272], obs["state"][0, :16])
            self.assertEqual(list(obs["state"][:, 274]), [0, 1])
            self.assertEqual(info["p1_lives"], 3)
            self.assertEqual(info["p2_lives"], 3)
            old = {k: v.copy() for k, v in obs.items()}
            env.step([3, 9])
            for key in old:
                np.testing.assert_array_equal(obs[key], old[key])

    def test_joint_seeded_replay_and_invalid_pair_atomicity(self):
        with CoopEnv(max_seconds=2) as left, CoopEnv(max_seconds=2) as right:
            left.reset(seed=145001)
            right.reset(seed=145001)
            for bad in ([0, 10], [-1, 0], [1.5, 0], [True, False], [0], [0, 0, 0]):
                before = left.digest()
                with self.assertRaises(ValueError):
                    left.step(bad)
                self.assertEqual(left.digest(), before)
            for i in range(40):
                a = [i % 10, (i + 3) % 10]
                obs, reward, term, trunc, info = left.step(a)
                other, r2, t2, tr2, info2 = right.step(a)
                self.assertEqual((reward, term, trunc, info), (r2, t2, tr2, info2))
                self.assertEqual(left.digest(), right.digest())
                for key in obs:
                    np.testing.assert_array_equal(obs[key], other[key])
                self.assertEqual(info["kills"], info["p1_kills"] + info["p2_kills"])
                self.assertEqual(info["deaths"], info["p1_deaths"] + info["p2_deaths"])
            self.assertTrue(trunc)
            self.assertFalse(term)
            with self.assertRaises(RuntimeError):
                left.step([0, 0])

    def test_single_actor_transfer_and_checkpoint_roundtrip(self):
        with TanksEnv() as single, CoopEnv() as coop:
            source = PPO("MultiInputPolicy", single, seed=14, n_steps=16, batch_size=16,
                         policy_kwargs={"features_extractor_class": TankTacticalFeatures,
                             "net_arch": {"pi": [64], "vf": [64]}, "normalize_images": False,
                             "share_features_extractor": False})
            actor = CoopActor()
            actor.initialize_from_single(source)
            obs, _ = coop.reset(seed=14001)
            for i in range(3):
                obs, *_ = coop.step([3 + 2 * i, 9 - 2 * i])
                batch = tensor_observation(obs)
                with torch.no_grad():
                    original = source.policy.get_distribution({"map": batch["map"][:, :14],
                                                               "state": batch["state"][:, :256]})
                    torch.testing.assert_close(actor.distribution(batch).logits,
                                               original.distribution.logits, atol=1e-6, rtol=1e-6)
            self.assertEqual(sum(p.numel() for p in actor.parameters()), 227282)
            calls = []
            hook = actor.base.register_forward_hook(lambda m, args, result: calls.append(result.shape[0]))
            actor(batch)
            hook.remove()
            self.assertEqual(calls, [2])
            with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory:
                path = Path(directory) / "actor.pt"
                save_actor(path, actor, world_steps=0)
                restored = load_actor(path)
                torch.testing.assert_close(restored(batch), actor(batch), rtol=0, atol=0)
                parent = Path(directory) / "parent.zip"
                source.save(parent)
                initialized = []
                for seed in (15001, 15051):
                    transferred, critic = initialize_models(seed, parent)
                    self.assertEqual(torch.initial_seed(), seed)
                    initialized.append((actor_hash(transferred), actor_hash(critic), torch.get_rng_state().clone()))
                self.assertEqual(initialized[0][0], initialized[1][0])
                self.assertNotEqual(initialized[0][1], initialized[1][1])
                self.assertFalse(torch.equal(initialized[0][2], initialized[1][2]))

    def test_team_gae_terminal_truncation_and_continuation(self):
        rewards = np.array([[1], [2], [3]], np.float32)
        values = np.zeros_like(rewards)
        next_values = np.array([[5], [7], [11]], np.float32)
        terminal = np.array([[0], [1], [0]], np.float32)
        boundary = np.array([[0], [1], [1]], np.float32)
        advantages, targets = team_gae(rewards, values, next_values, terminal, boundary, .9, 1)
        np.testing.assert_allclose(advantages[:, 0], [7.3, 2, 12.9], atol=1e-5)
        np.testing.assert_array_equal(targets, advantages)

    def test_unavailable_or_frozen_actor_has_zero_policy_gradient(self):
        logp = torch.tensor([-.7, -.7], requires_grad=True)
        loss, _, _ = actor_objective(logp, logp.detach().clone(), torch.zeros(2),
                                     torch.ones(2), torch.tensor([1., 0.]), .1, 0)
        loss.backward()
        torch.testing.assert_close(logp.grad, torch.tensor([-1., 0.]))

    def test_execution_masks_preserve_proposals_and_guarded_labels(self):
        with CoopEnv() as env:
            obs, _ = env.reset(seed=147)
        batch = {k: v[None].copy() for k, v in obs.items()}
        batch["state"][0, 0, 332] = 0
        batch["state"][0, 1, 332] = 1
        batch["state"][0, 1, :2] = [13 / 26, 23 / 26]
        proposal = np.array([[9, 5]])
        executed, valid = execute_actions(batch, proposal)
        np.testing.assert_array_equal(proposal, [[9, 5]])
        np.testing.assert_array_equal(executed, [[0, 4]])
        np.testing.assert_array_equal(valid, [[False, True]])

    def test_real_shared_actor_and_team_critic_update(self):
        actor, critic = CoopActor(), TeamCritic()
        with CoopEnv() as env:
            observations = []
            obs, _ = env.reset(seed=150)
            for _ in range(8):
                obs, *_ = env.step([3, 9])
                observations.append(obs)
        maps = np.stack([o["map"] for o in observations])
        states = np.stack([o["state"] for o in observations])
        with torch.no_grad():
            distribution = actor.distribution({"map": torch.from_numpy(maps).flatten(0, 1),
                                                "state": torch.from_numpy(states).flatten(0, 1)})
            actions = distribution.sample()
            logp = distribution.log_prob(actions).reshape(8, 2).numpy()
        actor_before = actor.cooperative.weight.detach().clone()
        critic_before = critic.value[0].weight.detach().clone()
        args = argparse.Namespace(epochs=2, batch_worlds=4, clip=.1, entropy=.005, target_kl=.03)
        stats = update(actor, critic, torch.optim.Adam(actor.parameters(), lr=3e-5),
                       torch.optim.Adam(critic.parameters(), lr=3e-4),
                       {"map": maps, "state": states, "mask": np.ones((8, 2), np.float32),
                        "actions": actions.reshape(8, 2).numpy(), "log_probs": logp,
                        "advantage": np.linspace(-1, 1, 8, dtype=np.float32),
                        "returns": np.linspace(0, 3, 8, dtype=np.float32)}, args, np.random.default_rng(14))
        self.assertTrue(np.isfinite(stats).all())
        self.assertFalse(torch.equal(actor_before, actor.cooperative.weight))
        self.assertFalse(torch.equal(critic_before, critic.value[0].weight))

    def test_cli_trains_and_saves_actual_native_rollout(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory:
            output = Path(directory) / "run"
            result = subprocess.run([sys.executable, "-m", "training.coop_train", "--output", str(output),
                                     "--world-steps", "128", "--envs", "2", "--rollout", "32", "--epochs", "1"],
                                    cwd=ROOT, text=True, capture_output=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            record = json.loads((output / "complete.json").read_text())
            self.assertEqual(record["world_steps"], 128)
            self.assertEqual(record["actor_proposals"], 256)
            self.assertEqual(record["native_ticks"], 384)
            self.assertGreater(record["valid_actor_samples"], 0)
            self.assertNotEqual(record["initial_actor_hash"], record["final_actor_hash"])
            self.assertGreater(record["cooperative_weight_norm"], 0)
            self.assertTrue(record["all_parameters_finite"])
            self.assertTrue((output / "training-state.pt").is_file())
            load_actor(output / "actor.pt")

    def test_partner_rng_does_not_shift_p2_sampling(self):
        actor = CoopActor()
        with CoopEnv() as env:
            obs, _ = env.reset(seed=149)
        obs["state"][:, 332] = 1
        shared = CoopController(actor, "self")
        different = CoopController(actor, "defender")
        shared.reset(191)
        different.reset(191)
        for _ in range(20):
            self.assertEqual(shared.predict(obs)[1], different.predict(obs)[1])
        self.assertEqual(shared.counts["p2_neural"], 20)
        self.assertEqual(different.counts["p2_neural"], 20)
        self.assertEqual(shared.counts["actor_batches"], 20)
        self.assertEqual(different.counts["p1_rule"], 20)

    def test_movement_diagnostic_excludes_stop_dead_and_short_runs(self):
        obs = {"state": np.zeros((2, 384), np.float32)}
        obs["state"][:, 332] = 1
        obs["state"][1, 0] = .5
        observer = MovementObserver()
        observer.step(obs, obs, [3, 0], .5)
        observer.step(obs, obs, [0, 0], .5)
        for _ in range(3):
            observer.step(obs, obs, [3, 0], .5)
        obs["state"][0, 332] = 0
        observer.step(obs, obs, [3, 0], .5)
        result = observer.finish()
        self.assertAlmostEqual(result["p1_stationary_seconds"], 1.5)
        self.assertEqual(result["p2_stationary_seconds"], 0)
        self.assertEqual(result["p1_stationary_near_ally_seconds"], 0)

    def test_stage_deck_covers_every_map_before_repeating(self):
        deck, repeated = StageDeck(range(1, 36), 15001), StageDeck(range(1, 36), 15001)
        first = [deck.next() for _ in range(35)]
        second = [deck.next() for _ in range(35)]
        self.assertEqual(sorted(first), list(range(1, 36)))
        self.assertEqual(sorted(second), list(range(1, 36)))
        self.assertNotEqual(first, second)
        self.assertEqual(first + second, [repeated.next() for _ in range(70)])

    def test_partner_mixture_is_fixed_per_episode_and_rotates_learning_slots(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory:
            path = Path(directory) / "history.pt"
            save_actor(path, CoopActor())
            pool = EpisodePartners(1, [path], [.5, .3, .2], 15051)
            kinds, roles = set(), set()
            for seed in range(200):
                pool.reset(0, seed)
                description = pool.description(0)
                kinds.add(description["partner_kind"])
                if description["partner_kind"] == "self":
                    self.assertEqual(description["learning_slots"], [0, 1])
                else:
                    self.assertEqual(len(description["learning_slots"]), 1)
                    roles.add(description["learning_slots"][0])
                self.assertEqual(description, pool.description(0))
            self.assertEqual(kinds, {"self", "historical", "defender"})
            self.assertEqual(roles, {0, 1})
            with self.assertRaises(ValueError):
                EpisodePartners(1, [], [0., 1., 0.], 15051)
            with self.assertRaises(ValueError):
                EpisodePartners(1, [path], [.5, .3, .3], 15051)

    def test_frozen_actions_do_not_consume_learner_rng_or_modify_observations(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory, CoopEnv() as env:
            path = Path(directory) / "history.pt"
            save_actor(path, CoopActor())
            pools = [EpisodePartners(1, [path], [0., 1., 0.], 15101) for _ in range(2)]
            obs, _ = env.reset(seed=151)
            batch = {key: value[None].copy() for key, value in obs.items()}
            batch["state"][..., 332] = 1
            before = {key: value.copy() for key, value in batch.items()}
            for pool in pools:
                pool.reset(0, 151)
            fixed = int(pools[0].fixed_slots[0])
            description = pools[0].description(0)
            state = torch.get_rng_state().clone()
            for _ in range(20):
                proposed = np.array([[9, 7]])
                left = pools[0].fill(batch, proposed)
                right = pools[1].fill(batch, proposed)
                np.testing.assert_array_equal(left, right)
                self.assertEqual(left[0, 1 - fixed], proposed[0, 1 - fixed])
                np.testing.assert_array_equal(proposed, [[9, 7]])
            self.assertTrue(torch.equal(state, torch.get_rng_state()))
            self.assertEqual(pools[0].description(0), description)
            for key in before:
                np.testing.assert_array_equal(before[key], batch[key])
            batch["state"][0, fixed, 332] = 0
            self.assertEqual(pools[0].fill(batch, proposed)[0, fixed], 0)
            self.assertEqual(pools[0].totals["historical_proposals"], 20)
            pools[0].verify_frozen()

    def test_learner_proposals_keep_frozen_slots_out_of_sampling_and_loss(self):
        with CoopEnv() as env:
            obs, _ = env.reset(seed=152)
        batch = {key: np.stack([value, value]) for key, value in obs.items()}
        learning = np.array([[True, False], [False, True]])
        actor = CoopActor()
        calls = []
        hook = actor.register_forward_hook(lambda m, args, result: calls.append(len(result)))
        actions, log_probs = propose_actions(actor, batch, learning)
        hook.remove()
        self.assertEqual(calls, [2])
        np.testing.assert_array_equal(actions[~learning], 0)
        np.testing.assert_array_equal(log_probs[~learning], 0)
        observation = {key: value.flatten(0, 1)[learning.reshape(-1)] for key, value in tensor_observation(batch).items()}
        expected = actor.distribution(observation).log_prob(torch.from_numpy(actions[learning]))
        np.testing.assert_allclose(log_probs[learning], expected.detach().numpy(), atol=1e-7)
        all_log_probs = torch.tensor(log_probs, requires_grad=True)
        loss, _, _ = actor_objective(all_log_probs, all_log_probs.detach(), torch.zeros_like(all_log_probs),
            torch.ones_like(all_log_probs), torch.tensor(learning, dtype=torch.float32), .1, 0)
        loss.backward()
        np.testing.assert_array_equal(all_log_probs.grad.numpy()[~learning], 0)

    def test_cli_cooperative_parent_and_critic_only_warmup(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory:
            directory = Path(directory)
            parent = directory / "parent.pt"
            source = CoopActor()
            save_actor(parent, source)
            before_hash = actor_hash(source)
            output = directory / "warmup"
            result = subprocess.run([sys.executable, "-m", "training.coop_train", "--output", str(output),
                "--coop-parent", str(parent), "--historical", str(parent), "--partner-mix", "0", "1", "0",
                "--balanced-stages", "--stages", "1", "2", "3", "--seed", "15301",
                "--world-steps", "128", "--critic-warmup-steps", "128", "--envs", "2", "--rollout", "32", "--epochs", "1"],
                cwd=ROOT, text=True, capture_output=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            record = json.loads((output / "complete.json").read_text())
            config = json.loads((output / "config.json").read_text())
            self.assertEqual(record["final_actor_hash"], before_hash)
            self.assertEqual(record["actor_updates"], 0)
            self.assertEqual(record["valid_actor_samples"], 0)
            self.assertGreater(record["ready_learner_samples"], 0)
            self.assertEqual(record["actor_proposals"], 128)
            self.assertGreater(record["historical_proposals"], 0)
            self.assertTrue(record["frozen_partners_unchanged"])
            self.assertEqual(config["sampling_seed"], 15301)
            saved = torch.load(output / "training-state.pt", map_location="cpu", weights_only=True)
            self.assertEqual(saved["actor_optimizer"]["state"], {})
            critic = TeamCritic()
            critic.load_state_dict(saved["critic"])
            self.assertNotEqual(actor_hash(critic), config["initial_critic_sha256"])

    def test_cli_scripted_partner_then_real_actor_updates(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory:
            output = Path(directory) / "mixed"
            result = subprocess.run([sys.executable, "-m", "training.coop_train", "--output", str(output),
                "--partner-mix", "0", "0", "1", "--world-steps", "256", "--critic-warmup-steps", "64",
                "--seed", "15351", "--envs", "2", "--rollout", "32", "--epochs", "1"],
                cwd=ROOT, text=True, capture_output=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            record = json.loads((output / "complete.json").read_text())
            self.assertNotEqual(record["initial_actor_hash"], record["final_actor_hash"])
            self.assertEqual(record["actor_updates"], 3)
            self.assertEqual(record["actor_proposals"], 256)
            self.assertEqual(record["partner_world_steps"]["defender"], 256)
            self.assertGreater(record["scripted_decisions"], 0)
            self.assertGreater(record["valid_actor_samples"], 0)
            self.assertGreater(record["ready_learner_samples"], record["valid_actor_samples"])


class TacticalRuleTests(unittest.TestCase):
    @staticmethod
    def scene():
        observation = {"map": np.zeros((15, 26, 26), np.float32),
                       "state": np.zeros(384, np.float32)}
        state = observation["state"]
        state[:4] = [5 / 26, 19 / 26, 0, -1]
        state[4], state[5], state[8], state[246], state[332] = .5, .3, 1, 1, 1
        state[16:19] = [1, 5 / 26, 5 / 26]
        return observation

    def test_tactical_holds_a_clear_firing_lane_without_charging(self):
        from training.rule_policy import TacticalDefender
        observation = self.scene()
        before = {key: value.copy() for key, value in observation.items()}
        rule = TacticalDefender()
        self.assertEqual(rule.predict(observation), 1)
        for key in before:
            np.testing.assert_array_equal(before[key], observation[key])
        observation["map"][4, 11:13, 4:6] = 1
        rule.reset()
        self.assertNotEqual(rule.predict(observation), 1)
        self.assertEqual(rule.counts["aimed_fire"], 0)

    def test_home_coverage_agrees_in_both_views_and_changes_with_position(self):
        from training.rule_policy import TacticalDefender
        with CoopEnv() as env:
            obs, _ = env.reset(seed=310)
        obs["state"][:, 332:334] = 1
        self.assertEqual([bool(TacticalDefender.home_role(s)) for s in obs["state"]], [False, True])
        obs["state"][0, 1] = obs["state"][1, 257] = 23 / 26
        obs["state"][1, 1] = obs["state"][0, 257] = 3 / 26
        self.assertEqual([bool(TacticalDefender.home_role(s)) for s in obs["state"]], [True, False])
        obs["state"][1, 333] = 0
        self.assertTrue(TacticalDefender.home_role(obs["state"][1]))

    def test_tactical_movement_respects_ally_water_and_hq(self):
        from training.rule_policy import TacticalDefender
        obs = self.scene()
        self.assertTrue(TacticalDefender.passable(obs, 1))
        obs["state"][333] = 1
        obs["state"][256:258] = [5 / 26, 17.3 / 26]
        self.assertFalse(TacticalDefender.passable(obs, 1))
        obs["state"][333] = 0
        obs["map"][5, 17:19, 4:6] = 1
        self.assertFalse(TacticalDefender.passable(obs, 1))
        obs["state"][9] = 1
        self.assertTrue(TacticalDefender.passable(obs, 1))
        obs["map"][9, 17:19, 4:6] = 1
        self.assertFalse(TacticalDefender.passable(obs, 1))

    def test_tactical_reset_and_unavailable_player(self):
        from training.rule_policy import TacticalDefender
        obs = self.scene()
        obs["map"][4, 11:13, 4:6] = 1
        rule, fresh = TacticalDefender(), TacticalDefender()
        for _ in range(24):
            rule.predict(obs)
        rule.reset()
        self.assertEqual(rule.predict(obs), fresh.predict(obs))
        obs["state"][12] = 1
        self.assertEqual(rule.predict(obs), 0)

    def test_tactical_joint_controller_uses_no_neural_actor_and_guard_is_retained(self):
        from training.hybrid import guard_fire
        from training.coop_env import player_view
        with CoopEnv(max_seconds=4) as env:
            obs, _ = env.reset(seed=311, options={"stage": 4})
            controller = CoopController(None, "self", rule_p2="tactical")
            for _ in range(80):
                before = {key: value.copy() for key, value in obs.items()}
                actions = controller.predict(obs)
                for slot in range(2):
                    self.assertEqual(actions[slot], guard_fire(player_view(obs, slot), int(actions[slot])))
                for key in before:
                    np.testing.assert_array_equal(before[key], obs[key])
                obs, _, term, trunc, _ = env.step(actions)
                if term or trunc:
                    break
            self.assertEqual(controller.counts["actor_batches"], 0)
            self.assertEqual(controller.counts["p1_neural"], 0)
            self.assertEqual(controller.counts["p2_neural"], 0)
            self.assertGreater(controller.counts["p1_rule"], 0)
            self.assertGreater(controller.counts["p2_rule"], 0)

    def test_real_tactical_evaluation_cli(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory:
            output = Path(directory) / "evaluation.json"
            result = subprocess.run([sys.executable, "-m", "training.coop_evaluate",
                "--rule-p2", "tactical", "--partner", "self", "--seed", "312",
                "--episodes", "1", "--max-seconds", "2", "--output", str(output)],
                cwd=ROOT, text=True, capture_output=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            record = json.loads(output.read_text())
            self.assertEqual(record["rule_p2"], "tactical")
            self.assertFalse(record["stochastic"])
            self.assertIsNone(record["model_sha256"])
            self.assertEqual(record["episodes"][0]["controller"]["actor_batches"], 0)


if __name__ == "__main__":
    unittest.main()
