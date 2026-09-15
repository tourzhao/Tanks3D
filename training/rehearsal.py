"""Training-only rehearsal of demonstrations and learner-visited corrections.

Supervised steps run before a new PPO rollout, so its stored action log
probabilities describe the policy that actually collected that rollout.
Evaluation loads only the resulting neural policy, with no teacher callback.
"""

from collections import deque
import json

import numpy as np
import torch
from stable_baselines3.common.callbacks import BaseCallback

from training.tutor import make_teacher


def unsafe_fire_labels(state):
    """Training-only negative labels using the fixed defender's HQ corridors.

    These deliberately conservative labels ignore intervening cover. They do
    not predict projectile collisions and are never used to mask policy actions.
    """
    x, z = state[:, 0] * 26, state[:, 1] * 26
    south = (x > 10.5) & (x < 15.5)
    west = (z > 22.5) & (x > 11)
    east = (z > 22.5) & (x < 15)
    stopped = (
        (south & (state[:, 3] > 0)) | (west & (state[:, 2] < 0)) | (east & (state[:, 2] > 0))
    )
    labels = torch.zeros((len(state), 10), dtype=torch.bool, device=state.device)
    labels[:, 1], labels[:, 5], labels[:, 7], labels[:, 9] = stopped, south, west, east
    return labels


def unsafe_fire_loss(probabilities, state):
    """Teach probability mass away from dangerous firing; inference stays free."""
    unsafe_mass = (probabilities * unsafe_fire_labels(state)).sum(dim=-1)
    return -torch.log1p(-unsafe_mass.clamp(max=1 - 1e-6)).mean()


class Rehearsal(BaseCallback):
    def __init__(
        self,
        output,
        data,
        batches=8,
        learning_rate=1e-4,
        seed=2027,
        nonfire_weight=1,
        safety_weight=0,
        teacher="defender",
        teacher_config=None,
    ):
        super().__init__()
        self.output, self.data = output, data
        self.batches, self.learning_rate = batches, learning_rate
        self.nonfire_weight = nonfire_weight
        self.safety_weight = safety_weight
        make_teacher(teacher, teacher_config)  # Fail early for invalid configurations.
        self.teacher = teacher
        self.teacher_config = teacher_config
        self.rng = np.random.default_rng(seed)
        self.memory = deque(maxlen=16)
        self.labels, self.history = [], []

    def _on_training_start(self):
        with np.load(self.data, allow_pickle=False) as data:
            self.demonstrations = {key: data[key] for key in ("map", "state", "action")}
        d = self.demonstrations
        count = len(d["action"])
        if (
            not count
            or d["map"].shape != (count, 14, 26, 26)
            or d["state"].shape != (count, 256)
            or d["map"].dtype != np.uint8
            or np.any((d["action"] < 0) | (d["action"] >= 10))
        ):
            raise ValueError("Invalid demonstration schema")
        self.teachers = [
            make_teacher(self.teacher, self.teacher_config)
            for _ in range(self.training_env.num_envs)
        ]
        self.optimizer = torch.optim.Adam(self.model.policy.parameters(), lr=self.learning_rate)

    def _on_step(self):
        # _last_obs is still the observation on which the current actions were
        # chosen. Auto-reset observations arrive separately in new_obs.
        observations = self.model._last_obs
        for i, teacher in enumerate(self.teachers):
            obs = {key: value[i] for key, value in observations.items()}
            self.labels.append(teacher.predict(obs))
            if self.locals["dones"][i]:
                teacher.reset()
        return True

    def _on_rollout_end(self):
        observations = self.model.rollout_buffer.observations
        count = len(self.labels)
        self.memory.append(
            {
                "map": np.rint(observations["map"] * 255)
                .astype(np.uint8)
                .reshape(count, 14, 26, 26),
                "state": observations["state"].reshape(count, 256).copy(),
                "action": np.asarray(self.labels, dtype=np.int64),
            }
        )
        self.labels = []

    def _on_rollout_start(self):
        previous_mode = self.model.policy.training
        self.model.policy.set_training_mode(True)
        losses = []
        safety_losses = []
        try:
            for _ in range(self.batches):
                # Half the labels retain the original demonstrations; half
                # cover recent learner mistakes, across up to 16 rollouts.
                sources = [self.demonstrations]
                if self.memory:
                    sources.append(self.memory[int(self.rng.integers(len(self.memory)))])
                parts = []
                for source in sources:
                    indices = self.rng.integers(len(source["action"]), size=256 // len(sources))
                    parts.append({key: source[key][indices] for key in source})
                batch = {
                    key: np.concatenate([part[key] for part in parts])
                    for key in ("map", "state", "action")
                }
                inputs = {
                    "map": torch.as_tensor(
                        batch["map"].astype(np.float32) / 255, device=self.model.device
                    ),
                    "state": torch.as_tensor(batch["state"], device=self.model.device),
                }
                labels = torch.as_tensor(batch["action"], device=self.model.device)
                weights = torch.where(labels % 2 == 0, self.nonfire_weight, 1.0)
                distribution = self.model.policy.get_distribution(inputs)
                imitation_loss = (
                    -(distribution.log_prob(labels) * weights).sum() / weights.sum()
                )
                safety_loss = unsafe_fire_loss(distribution.distribution.probs, inputs["state"])
                loss = imitation_loss + self.safety_weight * safety_loss
                self.optimizer.zero_grad()
                loss.backward()
                torch.nn.utils.clip_grad_norm_(self.model.policy.parameters(), 0.5)
                self.optimizer.step()
                losses.append(float(imitation_loss.item()))
                safety_losses.append(float(safety_loss.item()))
        finally:
            self.model.policy.set_training_mode(previous_mode)
        self.history.append(
            {
                "timesteps": self.num_timesteps,
                "cross_entropy": float(np.mean(losses)),
                "unsafe_fire_loss": float(np.mean(safety_losses)),
                "safety_weight": self.safety_weight,
                "replay_labels": sum(len(x["action"]) for x in self.memory),
                "supervised_batches": self.batches,
            }
        )
        (self.output / "rehearsal.json").write_text(json.dumps(self.history, indent=2))

    def _on_training_end(self):
        if self.memory:
            np.savez_compressed(self.output / "last-corrections.npz", **self.memory[-1])
