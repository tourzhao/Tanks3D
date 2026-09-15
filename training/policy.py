"""Small spatial encoder for terrain plus precise moving-entity state."""

import torch
from torch import nn
from stable_baselines3.common.torch_layers import BaseFeaturesExtractor


class TankFeatures(BaseFeaturesExtractor):
    def __init__(self, observation_space, features_dim=128):
        super().__init__(observation_space, features_dim)
        self.map_net = nn.Sequential(
            nn.Conv2d(14, 8, 3, 2, 1),
            nn.ReLU(),
            nn.Conv2d(8, 16, 3, 2, 1),
            nn.ReLU(),
            nn.Flatten(),
        )
        self.combine = nn.Sequential(nn.Linear(16 * 7 * 7 + 256, features_dim), nn.ReLU())

    def forward(self, observations):
        return self.combine(
            torch.cat((self.map_net(observations["map"]), observations["state"]), dim=1)
        )


class TankLocalFeatures(BaseFeaturesExtractor):
    """Full terrain plus a player-centered crop and relative entity coordinates.

    Every value is derived from the same observation; no planner, teacher action
    or future information enters model inference.
    """

    def __init__(self, observation_space, features_dim=128):
        super().__init__(observation_space, features_dim)
        self.map_net = nn.Sequential(
            nn.Conv2d(14, 8, 3, 2, 1),
            nn.ReLU(),
            nn.Conv2d(8, 16, 3, 2, 1),
            nn.ReLU(),
            nn.Flatten(),
        )
        values = torch.linspace(-2, 2, 5) / 13
        yy, xx = torch.meshgrid(values, values, indexing="ij")
        self.register_buffer("offsets", torch.stack((xx, yy), dim=-1)[None])
        self.combine = nn.Sequential(nn.Linear(784 + 350 + 256 + 58, features_dim), nn.ReLU())

    def forward(self, observations):
        state, terrain = observations["state"], observations["map"]
        position = state[:, :2]
        grid = self.offsets + (position * 2 - 1)[:, None, None, :]
        local = torch.nn.functional.grid_sample(
            terrain, grid, align_corners=False, padding_mode="border"
        ).flatten(1)
        enemies = state[:, 16:64].reshape(-1, 4, 12)
        relative_enemies = (enemies[:, :, 1:3] - position[:, None, :]) * enemies[:, :, :1]
        shells = state[:, 64:208].reshape(-1, 24, 6)
        relative_shells = (shells[:, :, 1:3] - position[:, None, :]) * shells[:, :, :1].abs()
        lane_offset = (position * 13).remainder(1) * 2 - 1
        return self.combine(
            torch.cat(
                (
                    self.map_net(terrain),
                    local,
                    state,
                    relative_enemies.flatten(1),
                    relative_shells.flatten(1),
                    lane_offset,
                ),
                dim=1,
            )
        )


class TankTacticalFeatures(TankLocalFeatures):
    """Tile-scale alignment cues, with no target selection or action rules."""

    def __init__(self, observation_space, features_dim=128):
        super().__init__(observation_space, features_dim)
        self.tactical = nn.Linear(38, features_dim, bias=False)

    def forward(self, observations):
        state = observations["state"]
        position = state[:, :2] * 26
        enemies = state[:, 16:64].reshape(-1, 4, 12)
        delta = enemies[:, :, 1:3] * 26 - position[:, None, :]
        entity = (
            torch.cat(
                (
                    (delta / 4).clamp(-1, 1),
                    (1 - delta.abs() / 0.75).clamp(0, 1),
                    delta.sign(),
                    enemies[:, :, 2:3],
                    (enemies[:, :, 2:3] * 26 - 25) / 26,
                ),
                dim=-1,
            )
            * enemies[:, :, :1]
        )
        # All 35 original maps have the headquarters at (13,25).
        base_delta = position.new_tensor([13.0, 25.0]) - position
        headquarters = torch.cat(
            (
                (base_delta / 4).clamp(-1, 1),
                (1 - base_delta.abs() / 2.5).clamp(0, 1),
                base_delta.sign(),
            ),
            dim=-1,
        )
        return super().forward(observations) + self.tactical(
            torch.cat((entity.flatten(1), headquarters), dim=-1)
        )


class TankWideFeatures(TankTacticalFeatures):
    """More terrain channels and hidden capacity, with unchanged observations."""

    def __init__(self, observation_space, features_dim=256):
        super().__init__(observation_space, features_dim)
        self.map_net = nn.Sequential(
            nn.Conv2d(14, 16, 3, 2, 1),
            nn.ReLU(),
            nn.Conv2d(16, 32, 3, 2, 1),
            nn.ReLU(),
            nn.Flatten(),
        )
        self.combine = nn.Sequential(nn.Linear(1568 + 350 + 256 + 58, features_dim), nn.ReLU())


class SpatialResidual(nn.Module):
    def __init__(self, channels):
        super().__init__()
        self.conv_in = nn.Conv2d(channels, channels, 3, padding=1)
        self.conv_out = nn.Conv2d(channels, channels, 3, padding=1)

    def forward(self, values):
        return values + self.conv_out(torch.relu(self.conv_in(values)))


class TankContextFeatures(TankWideFeatures):
    """Learn spatial context on the 7x7 terrain grid before flattening it."""

    def __init__(self, observation_space, features_dim=256):
        super().__init__(observation_space, features_dim)
        self.map_net = nn.Sequential(
            *list(self.map_net.children())[:-1],
            SpatialResidual(32),
            SpatialResidual(32),
            nn.Flatten(),
        )


class TankProjectileFeatures(TankWideFeatures):
    """Cheap relative-flight cues from observed enemy shells, without action rules.

    These are geometric features only: walls, future tank movement and collisions
    are not predicted. The original state still supplies each shell's velocity.
    """

    def __init__(self, observation_space, features_dim=256):
        super().__init__(observation_space, features_dim)
        self.projectiles = nn.Linear(24 * 4, features_dim, bias=False)

    @staticmethod
    def flight_features(state):
        shells = state[:, 64:208].reshape(-1, 24, 6)
        delta = (state[:, None, :2] - shells[:, :, 1:3]) * 26
        velocity = shells[:, :, 3:5] * 30
        # Native shells travel along cardinal axes; L1 magnitude is their speed.
        speed = velocity.abs().sum(dim=-1, keepdim=True).clamp_min(1e-6)
        direction = velocity / speed
        along = (delta * direction).sum(dim=-1, keepdim=True)
        lateral = delta[:, :, :1] * direction[:, :, 1:] - delta[:, :, 1:] * direction[:, :, :1]
        cues = torch.cat(
            (
                (along / 6).clamp(-1, 1),
                (lateral / 3).clamp(-1, 1),
                (1 - lateral.abs() / 0.75).clamp(0, 1),
                (along / speed).clamp(-1, 2) / 2,
            ),
            dim=-1,
        )
        return (cues * shells[:, :, :1].clamp(0, 1)).flatten(1)

    def forward(self, observations):
        return super().forward(observations) + self.projectiles(
            self.flight_features(observations["state"])
        )


def add_projectile_weights(source, target):
    """Start the added shell features at zero while preserving actor and critic."""
    if (
        type(source.pi_features_extractor) is not TankWideFeatures
        or type(target.pi_features_extractor) is not TankProjectileFeatures
        or source.net_arch != target.net_arch
        or source.pi_features_extractor.features_dim
        != target.pi_features_extractor.features_dim
        or (not source.share_features_extractor and target.share_features_extractor)
    ):
        raise ValueError("Projectile migration requires a matching widened policy")
    with torch.no_grad():
        for name, parameter in target.named_parameters():
            if name.endswith("projectiles.weight"):
                parameter.zero_()
    missing, unexpected = target.load_state_dict(source.state_dict(), strict=False)
    if unexpected or any(not name.endswith("projectiles.weight") for name in missing):
        raise ValueError("Incompatible projectile migration")


def add_context_weights(source, target):
    """Initialize extra spatial layers as identity, keeping the current policy."""
    if (
        type(source.pi_features_extractor) is not TankWideFeatures
        or type(target.pi_features_extractor) is not TankContextFeatures
        or source.net_arch != target.net_arch
        or source.pi_features_extractor.features_dim
        != target.pi_features_extractor.features_dim
        or (not source.share_features_extractor and target.share_features_extractor)
    ):
        raise ValueError("Spatial context migration requires a matching widened policy")
    with torch.no_grad():
        for name, parameter in target.named_parameters():
            if ".map_net.4.conv_out." in name or ".map_net.5.conv_out." in name:
                parameter.zero_()
    missing, unexpected = target.load_state_dict(source.state_dict(), strict=False)
    if unexpected or any(
        ".map_net.4." not in name and ".map_net.5." not in name for name in missing
    ):
        raise ValueError("Incompatible spatial context migration")


def widen_policy_weights(source, target):
    """Preserve a 128/64 tactical policy inside a 256/128 wider network.

    Existing units retain their inputs and outputs. New units keep their random
    initialization but start with zero outgoing connections into existing units,
    so training can use the extra capacity without discarding the old policy.
    The new architecture requires a fresh optimizer; its state is not migrated.
    """
    if (
        type(source.pi_features_extractor) is not TankTacticalFeatures
        or type(target.pi_features_extractor) is not TankWideFeatures
        or source.net_arch != {"pi": [64], "vf": [64]}
        or target.net_arch != {"pi": [128], "vf": [128]}
        or source.pi_features_extractor.features_dim != 128
        or target.pi_features_extractor.features_dim != 256
        or (not source.share_features_extractor and target.share_features_extractor)
    ):
        raise ValueError("Widening requires matching tactical 128/64 and wide 256/128 policies")
    before, after = source.state_dict(), target.state_dict()
    if before.keys() != after.keys():
        raise ValueError("Incompatible widened policy state")
    with torch.no_grad():
        for name, previous in before.items():
            expanded = after[name]
            if previous.shape == expanded.shape:
                expanded.copy_(previous)
            elif name.endswith(("map_net.0.weight", "map_net.0.bias", "map_net.2.bias")):
                expanded[: previous.shape[0]].copy_(previous)
            elif name.endswith("map_net.2.weight"):
                expanded[:16].zero_()
                expanded[:16, :8].copy_(previous)
            elif name.endswith("combine.0.weight"):
                expanded[:128].zero_()
                expanded[:128, :784].copy_(previous[:, :784])
                expanded[:128, 1568:].copy_(previous[:, 784:])
            elif name.endswith(("combine.0.bias", "tactical.weight")):
                expanded[:128].copy_(previous)
            elif name in (
                "mlp_extractor.policy_net.0.weight",
                "mlp_extractor.value_net.0.weight",
            ):
                expanded[:64].zero_()
                expanded[:64, :128].copy_(previous)
            elif name in ("mlp_extractor.policy_net.0.bias", "mlp_extractor.value_net.0.bias"):
                expanded[:64].copy_(previous)
            elif name in ("action_net.weight", "value_net.weight"):
                expanded.zero_()
                expanded[:, :64].copy_(previous)
            else:
                raise ValueError(f"Unsupported widened parameter: {name}")
    target.load_state_dict(after)


def copy_policy_weights(source, target):
    """Migrate an existing local policy with an initially zero tactical residual."""
    for name, parameter in target.named_parameters():
        if "tactical" in name:
            nn.init.zeros_(parameter)
    missing, unexpected = target.load_state_dict(source.state_dict(), strict=False)
    if unexpected or any("tactical.weight" not in name for name in missing):
        raise ValueError("Incompatible checkpoint migration")
