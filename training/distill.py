"""Compress a neural policy using its soft action probabilities on stored states."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import torch
from stable_baselines3 import PPO

from training.env import ROOT


def actor_parameters(policy):
    """Return each actor parameter once, without including the separate critic."""
    if policy.share_features_extractor:
        raise ValueError("Distillation requires a separate value encoder")
    return list(
        {
            id(parameter): parameter
            for module in (
                policy.pi_features_extractor,
                policy.mlp_extractor.policy_net,
                policy.action_net,
            )
            for parameter in module.parameters()
        }.values()
    )


def distillation_loss(policy, observations, target_probabilities):
    """KL(teacher || student), retaining the teacher's stochastic behavior."""
    distribution = policy.get_distribution(observations).distribution
    return torch.nn.functional.kl_div(
        distribution.logits, target_probabilities.detach(), reduction="batchmean"
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--teacher", type=Path, required=True)
    parser.add_argument("--student", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--epochs", type=int, default=8)
    parser.add_argument("--batch-size", type=int, default=256)
    parser.add_argument("--checkpoint-epochs", type=int, default=2)
    parser.add_argument("--lr", type=float, default=1e-4)
    parser.add_argument("--seed", type=int, default=7193)
    parser.add_argument("--threads", type=int, default=1)
    args = parser.parse_args()
    if min(args.epochs, args.batch_size, args.checkpoint_epochs, args.threads) < 1:
        parser.error("Positive epoch, batch, checkpoint and thread counts required")
    if args.lr <= 0 or not 0 <= args.seed < 60000:
        parser.error("Positive learning rate and a training seed below60000 required")
    if not args.output.resolve().is_relative_to((ROOT / "build").resolve()):
        parser.error("Generated outputs must be under build/")
    args.output.mkdir(parents=True, exist_ok=False)
    torch.set_num_threads(args.threads)
    teacher = PPO.load(args.teacher, device="cpu")
    student = PPO.load(args.student, device="cpu")
    if teacher.observation_space != student.observation_space:
        raise ValueError("Teacher and student observations must match")
    if teacher.action_space != student.action_space or student.action_space.n != 10:
        raise ValueError("Matching10-action policies required")
    # Loading a checkpoint restores its seed. Seed only after both loads.
    torch.manual_seed(args.seed)
    rng = np.random.default_rng(args.seed)
    teacher.policy.set_training_mode(False)
    teacher.policy.requires_grad_(False)
    student.policy.set_training_mode(True)
    parameters = actor_parameters(student.policy)
    optimizer = torch.optim.Adam(parameters, lr=args.lr)
    # A subsequent PPO continuation should start with fresh optimizer moments.
    student.policy.optimizer = student.policy.optimizer_class(
        student.policy.parameters(), lr=args.lr, **student.policy.optimizer_kwargs
    )
    with np.load(args.data, allow_pickle=False) as saved:
        maps, states = saved["map"], saved["state"]
    count = len(states)
    if (
        not count
        or maps.shape != (count, 14, 26, 26)
        or states.shape != (count, 256)
        or maps.dtype != np.uint8
        or states.dtype != np.float32
        or not np.isfinite(states).all()
    ):
        raise ValueError("Invalid stored observation schema")
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    config = {
        **{
            key: str(value) if isinstance(value, Path) else value
            for key, value in vars(args).items()
        },
        "algorithm": "soft_policy_distillation",
        "teacher_sha256": sha(args.teacher),
        "student_initial_sha256": sha(args.student),
        "data_sha256": sha(args.data),
        "source_sha256": {
            name: sha(ROOT / "training" / name) for name in ("distill.py", "policy.py")
        },
        "observations": count,
        "actor_parameters": sum(parameter.numel() for parameter in parameters),
        "encoder": type(student.policy.pi_features_extractor).__name__,
        "inherited_student_ppo_counter": student.num_timesteps,
        "new_ppo_decisions": 0,
        "ppo_optimizer_reset_for_future_resume": True,
        "demonstration_action_labels_used": False,
        "teacher_at_inference": False,
    }
    (args.output / "config.json").write_text(json.dumps(config, indent=2))

    def observations(indices):
        return {
            "map": torch.from_numpy(maps[indices].astype(np.float32) / 255),
            "state": torch.from_numpy(states[indices]),
        }

    started = time.monotonic()
    probabilities = np.empty((count, 10), dtype=np.float32)
    with torch.no_grad():
        for start in range(0, count, args.batch_size):
            indices = slice(start, min(start + args.batch_size, count))
            probabilities[indices] = teacher.policy.get_distribution(
                observations(indices)
            ).distribution.probs.numpy()
    if (
        not np.isfinite(probabilities).all()
        or np.any(probabilities < 0)
        or not np.allclose(probabilities.sum(axis=1), 1, atol=1e-6)
    ):
        raise ValueError("Invalid teacher action probabilities")
    np.save(args.output / "teacher-probabilities.npy", probabilities, allow_pickle=False)
    del teacher
    print(
        json.dumps({"event": "teacher-probabilities-cached", "observations": count}), flush=True
    )
    history = []
    for epoch in range(1, args.epochs + 1):
        order = rng.permutation(count)
        loss_sum = 0.0
        for start in range(0, count, args.batch_size):
            indices = order[start : start + args.batch_size]
            target = torch.from_numpy(probabilities[indices])
            loss = distillation_loss(student.policy, observations(indices), target)
            if not bool(torch.isfinite(loss)):
                raise RuntimeError("Nonfinite distillation loss")
            optimizer.zero_grad()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(parameters, 0.5)
            optimizer.step()
            loss_sum += loss.item() * len(indices)
        record = {
            "epoch": epoch,
            "label_presentations": epoch * count,
            "mean_training_batch_kl": loss_sum / count,
            "elapsed_seconds": time.monotonic() - started,
        }
        history.append(record)
        (args.output / "history.json").write_text(json.dumps(history, indent=2))
        if epoch % args.checkpoint_epochs == 0:
            student.save(args.output / f"epoch-{epoch}")
        print(json.dumps(record), flush=True)
    student.save(args.output / "model")
    (args.output / "result.json").write_text(
        json.dumps({**history[-1], "new_ppo_decisions": 0}, indent=2)
    )


if __name__ == "__main__":
    main()
