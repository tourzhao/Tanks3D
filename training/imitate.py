"""Supervised warm start from observation-only teachers; later learning is PPO.

Demonstrations are collected on seeds below 60000. Evaluation uses separate
seeds. No pretrained weights or external gameplay datasets are imported.
"""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import torch
from stable_baselines3 import PPO

from training.env import TanksEnv, ROOT
from training.tutor import TEACHER_NAMES, make_teacher
from training.policy import TankFeatures, TankLocalFeatures
from training.rehearsal import unsafe_fire_loss


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--samples", type=int, default=60000)
    p.add_argument("--epochs", type=int, default=8)
    p.add_argument("--seed", type=int, default=2026)
    p.add_argument("--stages", type=int, nargs="+", default=[1])
    p.add_argument("--teacher", choices=TEACHER_NAMES, default="defender")
    p.add_argument("--teacher-config", type=Path)
    p.add_argument(
        "--collect-only",
        action="store_true",
        help="Save demonstrations without fitting",
    )
    p.add_argument("--model", type=Path)
    p.add_argument(
        "--dagger",
        action="store_true",
        help="Visit learner states; label with the selected training teacher",
    )
    p.add_argument(
        "--dagger-stochastic",
        action="store_true",
        help="Sample learner actions during DAgger collection",
    )
    p.add_argument("--data", type=Path, help="Reuse an existing demonstration archive")
    p.add_argument("--local", action="store_true", help="Use player-centered spatial features")
    p.add_argument("--lr", type=float, default=3e-4)
    p.add_argument("--nonfire-weight", type=float, default=1)
    p.add_argument("--safety-weight", type=float, default=0)
    p.add_argument("--checkpoint-epochs", type=int, default=0)
    args = p.parse_args()
    if args.samples < 1 or args.epochs < 1:
        p.error("Positive sample and epoch counts are required")
    if (
        args.lr <= 0
        or args.nonfire_weight <= 0
        or args.safety_weight < 0
        or args.checkpoint_epochs < 0
    ):
        p.error(
            "Positive learning rate/nonfire weight and nonnegative safety/checkpoint values required"
        )
    if any(not 1 <= stage <= 35 for stage in args.stages):
        p.error("Stages must be in [1,35]")
    if not args.output.resolve().is_relative_to((ROOT / "build").resolve()):
        p.error("Outputs must be under build/")
    if args.dagger and args.model is None:
        p.error("--dagger requires --model")
    if args.dagger_stochastic and not args.dagger:
        p.error("--dagger-stochastic requires --dagger")
    if args.collect_only and args.data:
        p.error("--collect-only collects new data; it cannot be combined with --data")
    args.output.mkdir(parents=True, exist_ok=False)
    torch.set_num_threads(2)
    torch.manual_seed(args.seed)
    rng = np.random.default_rng(args.seed)
    env = TanksEnv()
    try:
        model = (
            PPO.load(args.model, env=env, device="cpu")
            if args.model
            else PPO(
                "MultiInputPolicy",
                env,
                learning_rate=3e-4,
                n_steps=512,
                batch_size=256,
                n_epochs=4,
                gamma=0.995,
                gae_lambda=0.95,
                ent_coef=0.01,
                seed=args.seed,
                device="cpu",
                verbose=0,
                policy_kwargs={
                    "features_extractor_class": (
                        TankLocalFeatures if args.local else TankFeatures
                    ),
                    "net_arch": {"pi": [64], "vf": [64]},
                    "normalize_images": False,
                },
            )
        )
        # PPO restoration initializes modules using the checkpoint's own seed.
        # Reapply the collection seed before any sampled learner actions.
        torch.manual_seed(args.seed)
        maps = np.empty((args.samples, 14, 26, 26), dtype=np.uint8)
        states = np.empty((args.samples, 256), dtype=np.float32)
        actions = np.empty(args.samples, dtype=np.int64)
        episode_seeds = []
        episode_stages = []
        bot = make_teacher(args.teacher, args.teacher_config)

        def reset():
            seed = int(rng.integers(2000, 60000))
            stage = args.stages[len(episode_seeds) % len(args.stages)]
            episode_seeds.append(seed)
            episode_stages.append(stage)
            bot.reset()
            return env.reset(seed=seed, options={"stage": stage})[0]

        obs = reset()
        started = time.monotonic()
        for i in range(0 if args.data else args.samples):
            action = bot.predict(obs)
            maps[i] = np.rint(obs["map"] * 255).astype(np.uint8)
            states[i], actions[i] = obs["state"], action
            if args.dagger and rng.random() < 0.75:
                executed = int(model.predict(obs, deterministic=not args.dagger_stochastic)[0])
            else:
                executed = action
            obs, _, term, trunc, _ = env.step(executed)
            if term or trunc:
                obs = reset()
            if (i + 1) % 10000 == 0:
                print(f"Collected {i+1}/{args.samples} teacher labels", flush=True)
        if args.data:
            with np.load(args.data, allow_pickle=False) as data:
                maps, states, actions = data["map"], data["state"], data["action"]
            args.samples = len(actions)
        # Keep compact labels/seed provenance; full data can be reconstructed
        # from the teacher and collection configuration, including learner hash.
        (args.output / "config.json").write_text(
            json.dumps(
                {
                    **vars(args),
                    "output": str(args.output),
                    "action_rng_seed": args.seed if args.dagger_stochastic else None,
                    "teacher_config": str(args.teacher_config) if args.teacher_config else None,
                    "teacher_config_sha256": (
                        hashlib.sha256(args.teacher_config.read_bytes()).hexdigest()
                        if args.teacher_config
                        else None
                    ),
                    "data": str(args.data) if args.data else None,
                    "data_sha256": (
                        hashlib.sha256(args.data.read_bytes()).hexdigest()
                        if args.data
                        else None
                    ),
                    "model": str(args.model) if args.model else None,
                    "episode_seeds": episode_seeds,
                    "episode_stages": episode_stages,
                    "teacher_source_sha256": {
                        name: hashlib.sha256(
                            (ROOT / "training" / name).read_bytes()
                        ).hexdigest()
                        for name in ("bot.py", "tutor.py")
                    },
                    "initial_model_sha256": (
                        hashlib.sha256(args.model.read_bytes()).hexdigest()
                        if args.model
                        else None
                    ),
                    "native_sha256": hashlib.sha256(
                        (ROOT / "build/ai/libtanks3d_training.dylib").read_bytes()
                    ).hexdigest(),
                    "collection_seconds": time.monotonic() - started,
                },
                indent=2,
            )
        )
        if not args.data:
            np.savez_compressed(
                args.output / "demonstrations.npz",
                map=maps,
                state=states,
                action=actions,
            )
        if args.collect_only:
            return
        model.policy.set_training_mode(True)
        optimizer = torch.optim.Adam(model.policy.parameters(), lr=args.lr)
        history = []
        for epoch in range(args.epochs):
            order = rng.permutation(args.samples)
            loss_sum, safety_sum, correct = 0.0, 0.0, 0
            for start in range(0, args.samples, 256):
                ix = order[start : start + 256]
                batch = {
                    "map": torch.from_numpy(maps[ix].astype(np.float32) / 255),
                    "state": torch.from_numpy(states[ix]),
                }
                labels = torch.from_numpy(actions[ix])
                distribution = model.policy.get_distribution(batch)
                weights = torch.where(labels % 2 == 0, args.nonfire_weight, 1.0)
                cross_entropy = -(distribution.log_prob(labels) * weights).sum() / weights.sum()
                safety = unsafe_fire_loss(distribution.distribution.probs, batch["state"])
                loss = cross_entropy + args.safety_weight * safety
                optimizer.zero_grad()
                loss.backward()
                torch.nn.utils.clip_grad_norm_(model.policy.parameters(), 0.5)
                optimizer.step()
                loss_sum += cross_entropy.item() * len(ix)
                safety_sum += safety.item() * len(ix)
                correct += (
                    (distribution.distribution.logits.argmax(dim=1) == labels).sum().item()
                )
            result = {
                "epoch": epoch + 1,
                "cross_entropy": loss_sum / args.samples,
                "unsafe_fire_loss": safety_sum / args.samples,
                "label_accuracy": correct / args.samples,
            }
            history.append(result)
            print(json.dumps(result), flush=True)
            if args.checkpoint_epochs and (epoch + 1) % args.checkpoint_epochs == 0:
                model.save(args.output / f"epoch-{epoch + 1}")
            (args.output / "learning.json").write_text(json.dumps(history, indent=2))
        model.save(args.output / "model")
        (args.output / "learning.json").write_text(json.dumps(history, indent=2))
    finally:
        env.close()


if __name__ == "__main__":
    main()
