"""Actor-only cooperative inference with independent per-slot sampling streams."""

import numpy as np
import torch

from training.bot import Defender
from training.hybrid import guard_fire
from training.coop_env import player_view
from training.coop_policy import tensor_observation
from training.rule_policy import TacticalDefender


class CoopController:
    def __init__(self, actor, partner="self", initial_actor=None, fire_guard=True,
                 rule_p2="defender"):
        if partner not in ("self", "initial", "defender", "tactical"):
            raise ValueError("Unknown P1 partner")
        if rule_p2 not in ("defender", "tactical"):
            raise ValueError("Unknown P2 rule policy")
        if partner == "initial" and initial_actor is None:
            raise ValueError("Initial P1 actor is required")
        self.actor, self.partner, self.initial_actor = actor, partner, initial_actor
        self.fire_guard = fire_guard
        first_rule = rule_p2 if partner == "self" else partner
        self.rule_names = [first_rule if first_rule == "tactical" else "defender", rule_p2]
        self.defenders = [TacticalDefender() if name == "tactical" else Defender()
                          for name in self.rule_names]
        self.reset(0)

    def reset(self, seed):
        self.generators = [torch.Generator().manual_seed(int(seed) + 123456789 + i * 10000019)
                           for i in range(2)]
        self.counts = {"decisions": 0, "actor_batches": 0, "p1_neural": 0, "p2_neural": 0,
                       "p1_rule": 0, "p2_rule": 0, "guard_changes": 0}
        for defender in self.defenders:
            defender.reset()

    @torch.no_grad()
    def predict(self, observation, deterministic=False):
        proposed = np.zeros(2, np.int64)
        current_slots = []
        for slot in range(2):
            if observation["state"][slot, 332] < .5:
                continue
            actor = self.actor
            if slot == 0:
                actor = self.initial_actor if self.partner == "initial" else (
                    None if self.partner in ("defender", "tactical") else self.actor)
            if actor is None:
                proposed[slot] = self.defenders[slot].predict(player_view(
                    observation, slot, legacy=self.rule_names[slot] == "defender"))
                self.counts[f"p{slot + 1}_rule"] += 1
            elif actor is self.actor:
                current_slots.append(slot)
            else:
                self._sample(actor, observation, [slot], proposed, deterministic)
        if current_slots:
            self._sample(self.actor, observation, current_slots, proposed, deterministic)
        executed = proposed.copy()
        if self.fire_guard:
            for slot in range(2):
                executed[slot] = guard_fire(player_view(observation, slot), int(executed[slot]))
        self.counts["guard_changes"] += int(np.count_nonzero(executed != proposed))
        self.counts["decisions"] += 1
        return executed

    def _sample(self, actor, observation, slots, proposed, deterministic):
        logits = actor(tensor_observation({key: value[slots] for key, value in observation.items()}))
        probabilities = logits.softmax(-1)
        for row, slot in enumerate(slots):
            proposed[slot] = int(logits[row].argmax() if deterministic else
                torch.multinomial(probabilities[row], 1, generator=self.generators[slot]).item())
            self.counts[f"p{slot + 1}_neural"] += 1
        self.counts["actor_batches"] += 1
