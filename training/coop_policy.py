"""A single transferable actor and a separate canonical-team value network."""

from pathlib import Path
import hashlib

import gymnasium as gym
import numpy as np
import torch
from torch import nn
from torch.distributions import Categorical

from training.policy import TankTacticalFeatures
from training.coop_env import CHANNELS, STATE_SIZE

FORMAT = "tanks3d-coop-v1"


def tensor_observation(observation):
    return {k: torch.as_tensor(v, dtype=torch.float32) for k, v in observation.items()}


class CoopActor(nn.Module):
    def __init__(self):
        super().__init__()
        space = gym.spaces.Dict({"map": gym.spaces.Box(0, 1, (14, 26, 26), np.float32),
                                 "state": gym.spaces.Box(-16, 16, (256,), np.float32)})
        self.base = TankTacticalFeatures(space, 128)
        self.cooperative = nn.Linear(STATE_SIZE - 256 + 49 + 25, 128, bias=False)
        nn.init.zeros_(self.cooperative.weight)
        self.head = nn.Sequential(nn.Linear(128, 64), nn.Tanh())
        self.action = nn.Linear(64, 10)

    def forward(self, observation):
        terrain, state = observation["map"], observation["state"]
        base = self.base({"map": terrain[:, :14], "state": state[:, :256]})
        ally = terrain[:, 14:15]
        global_ally = torch.nn.functional.adaptive_avg_pool2d(ally, (7, 7)).flatten(1)
        grid = self.base.offsets + (state[:, :2] * 2 - 1)[:, None, None, :]
        local_ally = torch.nn.functional.grid_sample(ally, grid, align_corners=False,
                                                   padding_mode="border").flatten(1)
        extra = self.cooperative(torch.cat((state[:, 256:], global_ally, local_ally), dim=1))
        return self.action(self.head(base + extra))

    def distribution(self, observation):
        return Categorical(logits=self(observation))

    def initialize_from_single(self, model):
        policy = model.policy
        if (type(policy.pi_features_extractor) is not TankTacticalFeatures
                or policy.net_arch["pi"] != [64]):
            raise ValueError("Transfer requires the retained 128/64 tactical actor")
        self.base.load_state_dict(policy.pi_features_extractor.state_dict())
        self.head.load_state_dict(policy.mlp_extractor.policy_net.state_dict())
        self.action.load_state_dict(policy.action_net.state_dict())
        nn.init.zeros_(self.cooperative.weight)


class TeamCritic(nn.Module):
    def __init__(self):
        super().__init__()
        self.map_net = nn.Sequential(nn.Conv2d(CHANNELS, 8, 3, 2, 1), nn.ReLU(),
                                     nn.Conv2d(8, 16, 3, 2, 1), nn.ReLU(), nn.Flatten())
        self.value = nn.Sequential(nn.Linear(784 + STATE_SIZE, 128), nn.Tanh(), nn.Linear(128, 1))

    def forward(self, canonical_observation):
        features = torch.cat((self.map_net(canonical_observation["map"]),
                              canonical_observation["state"]), dim=1)
        return self.value(features).squeeze(-1)


def save_actor(path, actor, **metadata):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    torch.save({"format": FORMAT, "actor": actor.state_dict(), "metadata": metadata}, path)


def load_actor(path):
    saved = torch.load(path, map_location="cpu", weights_only=True)
    if saved.get("format") != FORMAT:
        raise ValueError("Unsupported cooperative actor format")
    actor = CoopActor()
    actor.load_state_dict(saved["actor"], strict=True)
    actor.eval()
    return actor


def actor_hash(actor):
    digest = hashlib.sha256()
    for name, value in actor.state_dict().items():
        digest.update(name.encode())
        digest.update(value.detach().cpu().numpy().tobytes())
    return digest.hexdigest()
