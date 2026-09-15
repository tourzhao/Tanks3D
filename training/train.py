"""Train PPO on the real single-player game; all outputs stay under build/."""

import argparse
import hashlib
import json
import math
from pathlib import Path
import time

import gymnasium as gym
import numpy as np
import torch
from stable_baselines3 import PPO
from stable_baselines3.common.callbacks import BaseCallback
from stable_baselines3.common.monitor import Monitor
from stable_baselines3.common.vec_env import DummyVecEnv

from training.env import TanksEnv, ROOT
from training.hybrid import HYBRID_MODES
from training.hybrid_env import HybridTrainingEnv
from training.rewards import DeathPenalty
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
from training.rehearsal import Rehearsal
from training.tutor import TEACHER_NAMES


class Progress(BaseCallback):
    def __init__(self, output, checkpoint_steps=65536):
        super().__init__()
        self.output, self.started, self.last_report = output, time.monotonic(), 0
        self.episodes = []
        self.base_steps = 0
        self.last_checkpoint = 0
        self.checkpoint_steps = checkpoint_steps
        self.reward_totals = dict.fromkeys(("native", "extra_death", "shaped"), 0.0)
        self.hybrid_counts = dict.fromkeys(
            (
                "decisions",
                "neural_branch_decisions",
                "defender_decisions",
                "fire_suppressions",
                "rule_decisions",
                "changed_actions",
                "defense_entries",
            ),
            0,
        )

    def _on_training_start(self):
        self.base_steps = self.model.num_timesteps

    def _on_rollout_start(self):
        # Preserve completed optimizer updates at reproducible step counts.
        completed = self.num_timesteps - self.base_steps
        if completed // self.checkpoint_steps > self.last_checkpoint:
            self.model.save(self.output / f"step-{self.num_timesteps}")
            self.last_checkpoint = completed // self.checkpoint_steps

    def _on_step(self):
        for info in self.locals["infos"]:
            terms = info.get(
                "reward_terms",
                {"native": info["reward"], "extra_death": 0.0, "shaped": info["reward"]},
            )
            for key, value in terms.items():
                self.reward_totals[key] += value
            if "hybrid_action" in info:
                action = info["hybrid_action"]
                self.hybrid_counts["decisions"] += 1
                self.hybrid_counts["neural_branch_decisions"] += int(not action["defender"])
                self.hybrid_counts["defender_decisions"] += int(action["defender"])
                self.hybrid_counts["fire_suppressions"] += int(action["fire_suppressed"])
                self.hybrid_counts["rule_decisions"] += int(
                    action["defender"] or action["fire_suppressed"]
                )
                self.hybrid_counts["changed_actions"] += int(
                    action["executed"] != action["proposed"]
                )
                self.hybrid_counts["defense_entries"] += int(action["defense_entry"])
            if "episode" in info:
                self.episodes.append(
                    {
                        "timesteps": self.num_timesteps,
                        **info["episode"],
                        **{
                            k: info[k]
                            for k in (
                                "kills",
                                "deaths",
                                "won",
                                "base_alive",
                                "own_base_hits",
                                "ticks",
                                "episode_seed",
                                "stage",
                            )
                        },
                        **(
                            {"hybrid_training": info["hybrid_training"]}
                            if "hybrid_training" in info
                            else {}
                        ),
                        **(
                            {"episode_reward_terms": info["episode_reward_terms"]}
                            if "episode_reward_terms" in info
                            else {}
                        ),
                    }
                )
        if time.monotonic() - self.last_report >= 30:
            elapsed = time.monotonic() - self.started
            recent = self.episodes[-30:]
            print(
                json.dumps(
                    {
                        "steps": self.num_timesteps,
                        "seconds": round(elapsed),
                        "steps_per_second": round(
                            (self.num_timesteps - self.base_steps) / max(elapsed, 0.01)
                        ),
                        "recent_kills": (
                            round(float(np.mean([e["kills"] for e in recent])), 2)
                            if recent
                            else 0
                        ),
                        "recent_wins": sum(e["won"] for e in recent),
                        "episodes": len(self.episodes),
                        "reward_totals_unscaled": self.reward_totals,
                        **(
                            {"hybrid_training": self.hybrid_counts}
                            if self.hybrid_counts["decisions"]
                            else {}
                        ),
                    }
                ),
                flush=True,
            )
            self.last_report = time.monotonic()
            (self.output / "episodes.json").write_text(json.dumps(self.episodes))
            self.model.save(self.output / "latest")
        return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--steps", type=int, default=524288)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--stages", type=int, nargs="+", default=[1])
    parser.add_argument("--envs", type=int, default=4)
    parser.add_argument("--max-seconds", type=int, default=120)
    parser.add_argument("--resume", type=Path)
    parser.add_argument("--lr", type=float, default=3e-4)
    parser.add_argument("--entropy", type=float, default=0.02)
    parser.add_argument("--gamma", type=float, default=0.995)
    parser.add_argument("--gae", type=float, default=0.95)
    parser.add_argument("--reward-scale", type=float, default=1)
    parser.add_argument(
        "--extra-death-penalty",
        type=float,
        default=0,
        help="Extra training-only cost per player destruction, before reward scaling",
    )
    parser.add_argument("--separate-value", action="store_true")
    parser.add_argument("--tactical", action="store_true")
    parser.add_argument("--wide", action="store_true", help="Use a 256-feature, 128-head actor")
    parser.add_argument(
        "--context", action="store_true", help="Add spatial layers to a widened policy"
    )
    parser.add_argument(
        "--projectiles",
        action="store_true",
        help="Add relative-flight cues to a widened policy",
    )
    parser.add_argument("--threads", type=int, default=2)
    parser.add_argument("--checkpoint-steps", type=int, default=65536)
    parser.add_argument(
        "--hybrid-mode",
        choices=HYBRID_MODES,
        help="Execute the existing hybrid rules during PPO rollouts (opt-in)",
    )
    parser.add_argument("--guidance-data", type=Path)
    parser.add_argument("--guidance-batches", type=int, default=8)
    parser.add_argument("--guidance-lr", type=float, default=1e-4)
    parser.add_argument("--guidance-nonfire-weight", type=float, default=1)
    parser.add_argument("--guidance-safety-weight", type=float, default=0)
    parser.add_argument("--guidance-teacher", choices=TEACHER_NAMES, default="defender")
    parser.add_argument("--guidance-teacher-config", type=Path)
    args = parser.parse_args()
    if args.steps < 1 or args.envs < 1 or not 0 <= args.seed < args.seed + args.envs <= 60000:
        parser.error("Positive steps/envs and a training seed below 60000 are required")
    if args.threads < 1 or args.checkpoint_steps < 1:
        parser.error("Positive thread count and checkpoint interval are required")
    if args.context and args.projectiles:
        parser.error("Context and projectile encoders are separate experiments")
    if args.lr <= 0 or args.entropy < 0 or not 0 < args.gamma <= 1:
        parser.error("Invalid learning rate, entropy coefficient or discount")
    if not 0 < args.gae <= 1 or args.reward_scale <= 0:
        parser.error("Invalid GAE coefficient or positive reward scale")
    if not math.isfinite(args.extra_death_penalty) or args.extra_death_penalty < 0:
        parser.error("Extra death penalty must be finite and nonnegative")
    if args.guidance_batches < 1 or args.guidance_lr <= 0 or args.guidance_nonfire_weight <= 0:
        parser.error("Positive guidance batch count and learning rate required")
    if args.guidance_safety_weight < 0 or (
        args.guidance_safety_weight and not args.guidance_data
    ):
        parser.error("Nonnegative safety weight requires guidance data when enabled")
    if args.guidance_teacher_config and not args.guidance_data:
        parser.error("A teacher curriculum requires guidance data")
    if not args.output.resolve().is_relative_to((ROOT / "build").resolve()):
        parser.error("Generated outputs must be under build/")
    args.output.mkdir(parents=True, exist_ok=False)
    torch.set_num_threads(args.threads)

    def make_env():
        game = TanksEnv(
            stages=args.stages,
            max_seconds=args.max_seconds,
            seed_limit=60000,
        )
        if args.hybrid_mode:
            game = HybridTrainingEnv(game, args.hybrid_mode)
        if args.extra_death_penalty:
            game = DeathPenalty(game, args.extra_death_penalty)
        return Monitor(
            gym.wrappers.TransformReward(game, lambda reward: reward * args.reward_scale)
        )

    env = DummyVecEnv([make_env for _ in range(args.envs)])
    config = {
        **vars(args),
        "output": str(args.output),
        "resume": str(args.resume) if args.resume else None,
        "resume_sha256": (
            hashlib.sha256(args.resume.read_bytes()).hexdigest() if args.resume else None
        ),
        "guidance_data": str(args.guidance_data) if args.guidance_data else None,
        "guidance_teacher_config": (
            str(args.guidance_teacher_config) if args.guidance_teacher_config else None
        ),
        "guidance_teacher_config_sha256": (
            hashlib.sha256(args.guidance_teacher_config.read_bytes()).hexdigest()
            if args.guidance_teacher_config
            else None
        ),
        "guidance_memory_rollouts": 16 if args.guidance_data else 0,
        "teacher_source_sha256": {
            name: hashlib.sha256((ROOT / "training" / name).read_bytes()).hexdigest()
            for name in ("bot.py", "tutor.py")
        },
        "training_source_sha256": {
            name: hashlib.sha256((ROOT / "training" / name).read_bytes()).hexdigest()
            for name in (
                "train.py",
                "policy.py",
                "env.py",
                "rehearsal.py",
                "tutor.py",
                "bot.py",
                "hybrid.py",
                "hybrid_env.py",
                "rewards.py",
            )
        },
        "guidance_data_sha256": (
            hashlib.sha256(args.guidance_data.read_bytes()).hexdigest()
            if args.guidance_data
            else None
        ),
        "native_sha256": hashlib.sha256(
            (ROOT / "build/ai/libtanks3d_training.dylib").read_bytes()
        ).hexdigest(),
        "schema": 1,
        "action_repeat": 3,
        "simulation_hz": 60,
        "lives": 3,
        "training_seed_range": [0, 59999],
        "reward_shaping": {
            "extra_penalty_per_player_destruction": args.extra_death_penalty,
            "native_destruction_event_reward": -3,
            "shaped_destruction_event_reward": -3 - args.extra_death_penalty,
            "applied_before_reward_scale": True,
            "changes_gameplay_or_observation": False,
            "all_other_event_and_terminal_rewards_unchanged": True,
            "inference_use": False,
        },
        "hybrid_training": (
            {
                "configuration": env.envs[0].get_wrapper_attr("controller").configuration(),
                "rollout_actions": "original neural proposals and their log probabilities",
                "executed_actions": "same fixed HybridPolicy transform as inference",
                "network_proposals_during_takeover": True,
                "additional_observation_features": 0,
            }
            if args.hybrid_mode
            else None
        ),
    }
    (args.output / "config.json").write_text(json.dumps(config, indent=2))
    try:
        if args.resume:
            model = PPO.load(
                args.resume,
                env=env,
                device="cpu",
                learning_rate=args.lr,
                ent_coef=args.entropy,
                gamma=args.gamma,
                gae_lambda=args.gae,
                target_kl=0.03,
            )
            needs_tactical = args.tactical and type(model.policy.pi_features_extractor) not in (
                TankTacticalFeatures,
                TankWideFeatures,
                TankContextFeatures,
                TankProjectileFeatures,
            )
            needs_wide = args.wide and type(model.policy.pi_features_extractor) not in (
                TankWideFeatures,
                TankContextFeatures,
                TankProjectileFeatures,
            )
            needs_context = (
                args.context
                and type(model.policy.pi_features_extractor) is not TankContextFeatures
            )
            needs_projectiles = (
                args.projectiles
                and type(model.policy.pi_features_extractor) is not TankProjectileFeatures
            )
            if (
                (args.separate_value and model.policy.share_features_extractor)
                or needs_tactical
                or needs_wide
                or needs_context
                or needs_projectiles
            ):
                previous = model
                if (needs_context or needs_projectiles) and type(
                    previous.policy.pi_features_extractor
                ) is not TankWideFeatures:
                    raise ValueError(
                        "Context/projectile continuation requires an already widened checkpoint"
                    )
                if (
                    needs_tactical
                    and type(previous.policy.pi_features_extractor) is not TankLocalFeatures
                ):
                    raise ValueError("Tactical migration requires a local encoder checkpoint")
                policy_kwargs = {
                    **previous.policy_kwargs,
                    "share_features_extractor": previous.policy.share_features_extractor
                    and not args.separate_value,
                }
                if needs_tactical:
                    policy_kwargs["features_extractor_class"] = TankTacticalFeatures
                if needs_wide:
                    policy_kwargs["features_extractor_class"] = TankWideFeatures
                    policy_kwargs["features_extractor_kwargs"] = {"features_dim": 256}
                    policy_kwargs["net_arch"] = {"pi": [128], "vf": [128]}
                if needs_context:
                    policy_kwargs["features_extractor_class"] = TankContextFeatures
                if needs_projectiles:
                    policy_kwargs["features_extractor_class"] = TankProjectileFeatures
                model = PPO(
                    "MultiInputPolicy",
                    env,
                    device="cpu",
                    seed=args.seed,
                    learning_rate=args.lr,
                    ent_coef=args.entropy,
                    gamma=args.gamma,
                    gae_lambda=args.gae,
                    target_kl=0.03,
                    n_steps=previous.n_steps,
                    batch_size=previous.batch_size,
                    n_epochs=previous.n_epochs,
                    policy_kwargs=policy_kwargs,
                )
                if needs_context:
                    add_context_weights(previous.policy, model.policy)
                elif needs_projectiles:
                    add_projectile_weights(previous.policy, model.policy)
                elif needs_wide:
                    widen_policy_weights(previous.policy, model.policy)
                else:
                    copy_policy_weights(previous.policy, model.policy)
                model.num_timesteps = previous.num_timesteps
                model._n_updates = previous._n_updates
                config["optimizer_reset_for_encoder_migration"] = True
                (args.output / "config.json").write_text(json.dumps(config, indent=2))
            model.set_random_seed(args.seed)
        else:
            model = PPO(
                "MultiInputPolicy",
                env,
                learning_rate=args.lr,
                n_steps=512,
                batch_size=256,
                n_epochs=4,
                gamma=args.gamma,
                gae_lambda=args.gae,
                ent_coef=args.entropy,
                seed=args.seed,
                policy_kwargs={
                    "features_extractor_class": (
                        TankProjectileFeatures
                        if args.projectiles
                        else (
                            TankContextFeatures
                            if args.context
                            else (
                                TankWideFeatures
                                if args.wide
                                else TankTacticalFeatures if args.tactical else TankFeatures
                            )
                        )
                    ),
                    "net_arch": (
                        {"pi": [128], "vf": [128]}
                        if args.wide or args.context or args.projectiles
                        else {"pi": [64], "vf": [64]}
                    ),
                    "normalize_images": False,
                    "share_features_extractor": not args.separate_value,
                },
                device="cpu",
                verbose=0,
            )
        config["actual_encoder"] = type(model.policy.pi_features_extractor).__name__
        config["actual_separate_value"] = not model.policy.share_features_extractor
        (args.output / "config.json").write_text(json.dumps(config, indent=2))
        callback = Progress(args.output, args.checkpoint_steps)
        callbacks = [callback]
        if args.guidance_data:
            callbacks.append(
                Rehearsal(
                    args.output,
                    args.guidance_data,
                    args.guidance_batches,
                    args.guidance_lr,
                    args.seed + 2027,
                    args.guidance_nonfire_weight,
                    args.guidance_safety_weight,
                    args.guidance_teacher,
                    args.guidance_teacher_config,
                )
            )
        start = time.monotonic()
        model.learn(
            total_timesteps=args.steps,
            callback=callbacks,
            reset_num_timesteps=not bool(args.resume),
        )
        model.save(args.output / "model")
        (args.output / "episodes.json").write_text(json.dumps(callback.episodes))
        (args.output / "result.json").write_text(
            json.dumps(
                {
                    "timesteps": model.num_timesteps,
                    "elapsed_seconds": time.monotonic() - start,
                    "episodes": len(callback.episodes),
                    "hybrid_training": callback.hybrid_counts if args.hybrid_mode else None,
                    "reward_totals_unscaled": callback.reward_totals,
                },
                indent=2,
            )
        )
    finally:
        env.close()


if __name__ == "__main__":
    main()
