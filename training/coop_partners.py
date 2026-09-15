"""Training-only frozen partners and a balanced deck of original game stages."""

from pathlib import Path
import hashlib

import numpy as np
import torch

from training.bot import Defender
from training.coop_env import player_view
from training.coop_policy import load_actor, tensor_observation, actor_hash

KINDS = ("self", "historical", "defender")


class StageDeck:
    def __init__(self, stages, seed):
        self.stages = tuple(stages)
        self.rng = np.random.default_rng(np.random.SeedSequence([seed, 302]))
        self.remaining = []

    def next(self):
        if not self.remaining:
            self.remaining = self.rng.permutation(self.stages).tolist()
        return int(self.remaining.pop())


class EpisodePartners:
    def __init__(self, worlds, paths, probabilities, seed):
        probabilities = np.asarray(probabilities, dtype=float)
        if (probabilities.shape != (3,) or not np.isfinite(probabilities).all()
                or (probabilities < 0).any() or abs(probabilities.sum() - 1) > 1e-8
                or (probabilities[1] > 0 and not paths)):
            raise ValueError("Partner probabilities must sum to one; history requires checkpoints")
        self.probabilities = probabilities
        self.rng = np.random.default_rng(np.random.SeedSequence([seed, 201]))
        self.paths = [Path(p) for p in paths]
        self.actors = [load_actor(path).requires_grad_(False) for path in self.paths]
        self.hashes = [actor_hash(actor) for actor in self.actors]
        self.file_hashes = [hashlib.sha256(path.read_bytes()).hexdigest() for path in self.paths]
        self.kinds = ["self"] * worlds
        self.fixed_slots = np.full(worlds, -1, dtype=int)
        self.indices = np.full(worlds, -1, dtype=int)
        self.generators = [torch.Generator() for _ in range(worlds)]
        self.defenders = [Defender() for _ in range(worlds)]
        self.totals = {"historical_proposals": 0, "scripted_decisions": 0}

    def reset(self, world, episode_seed):
        kind = KINDS[int(self.rng.choice(3, p=self.probabilities))]
        self.kinds[world] = kind
        self.fixed_slots[world] = int(self.rng.integers(2)) if kind != "self" else -1
        self.indices[world] = int(self.rng.integers(len(self.actors))) if kind == "historical" else -1
        self.generators[world].manual_seed(int(episode_seed) + 1939000001 + world * 10000019)
        self.defenders[world].reset()

    def learner_mask(self):
        mask = np.ones((len(self.kinds), 2), dtype=bool)
        for world, slot in enumerate(self.fixed_slots):
            if slot >= 0:
                mask[world, slot] = False
        return mask

    def description(self, world):
        index = self.indices[world]
        return {"partner_kind": self.kinds[world],
                "learning_slots": np.flatnonzero(self.learner_mask()[world]).tolist(),
                "partner_checkpoint": str(self.paths[index]) if index >= 0 else None,
                "partner_sha256": self.file_hashes[index] if index >= 0 else None}

    @torch.no_grad()
    def fill(self, observations, learner_proposals):
        """Only replace frozen slots; the caller keeps learner proposal/log-prob labels."""
        joint = np.array(learner_proposals, copy=True)
        for world, slot in enumerate(self.fixed_slots):
            if slot < 0:
                continue
            joint[world, slot] = 0
            observation = {key: value[world] for key, value in observations.items()}
            if observation["state"][slot, 332] < .5:
                continue
            view = player_view(observation, int(slot))
            if self.kinds[world] == "defender":
                joint[world, slot] = self.defenders[world].predict(player_view(observation, int(slot), legacy=True))
                self.totals["scripted_decisions"] += 1
            else:
                logits = self.actors[self.indices[world]](tensor_observation({k: v[None] for k, v in view.items()}))
                joint[world, slot] = int(torch.multinomial(logits.softmax(-1)[0], 1,
                    generator=self.generators[world]).item())
                self.totals["historical_proposals"] += 1
        return joint

    def verify_frozen(self):
        if [actor_hash(actor) for actor in self.actors] != self.hashes:
            raise RuntimeError("A frozen partner's weights changed")
        if any(p.grad is not None for actor in self.actors for p in actor.parameters()):
            raise RuntimeError("A frozen partner received a gradient")
