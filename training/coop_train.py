"""Shared-actor PPO, one team critic and one transition per native world step."""

import argparse
from contextlib import ExitStack
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import torch
from torch import nn

from training.env import ROOT
from training.hybrid import guard_fire
from training.coop_env import CoopEnv, player_view
from training.coop_policy import (CoopActor, TeamCritic, actor_hash, save_actor,
                                  tensor_observation, load_actor)
from training.coop_partners import EpisodePartners, StageDeck


def team_gae(rewards, values, next_values, terminated, boundaries, gamma, gae_lambda):
    """A player death is not a boundary; a time limit bootstraps before reset."""
    advantage = np.zeros_like(rewards)
    carry = np.zeros(rewards.shape[1], np.float32)
    for t in range(len(rewards) - 1, -1, -1):
        delta = rewards[t] + gamma * next_values[t] * (1 - terminated[t]) - values[t]
        carry = delta + gamma * gae_lambda * (1 - boundaries[t]) * carry
        advantage[t] = carry
    return advantage, advantage + values


def actor_objective(log_probs, old_log_probs, entropy, advantage, mask, clip, entropy_weight):
    denominator = mask.sum().clamp_min(1)
    log_ratio = log_probs - old_log_probs
    ratio = log_ratio.exp()
    loss = -torch.minimum(ratio * advantage, ratio.clamp(1 - clip, 1 + clip) * advantage)
    policy_loss = (loss * mask).sum() / denominator
    mean_entropy = (entropy * mask).sum() / denominator
    kl = (((ratio - 1) - log_ratio) * mask).sum() / denominator
    return policy_loss - entropy_weight * mean_entropy, mean_entropy, kl


def stack_observations(observations):
    return {key: np.stack([o[key] for o in observations]) for key in ("map", "state")}


def execute_actions(observations, proposed, fire_guard=True):
    """Proposal labels remain intact; unavailable slots and HQ guard affect execution."""
    executed = np.array(proposed, copy=True)
    ready = observations["state"][..., 332] > .5
    executed[~ready] = 0
    if fire_guard:
        for world in range(len(executed)):
            obs = {key: value[world] for key, value in observations.items()}
            for slot in range(2):
                if ready[world, slot]:
                    executed[world, slot] = guard_fire(player_view(obs, slot), int(executed[world, slot]))
    return executed, ready


@torch.no_grad()
def propose_actions(actor, observations, learning):
    """Sample only learning slots; masked fixed slots have neutral stored labels."""
    actions = np.zeros(learning.shape, np.int64)
    log_probs = np.zeros(learning.shape, np.float32)
    distribution = actor.distribution({key: value.flatten(0, 1)[learning.reshape(-1)]
                                       for key, value in tensor_observation(observations).items()})
    sampled = distribution.sample()
    actions[learning] = sampled.numpy()
    log_probs[learning] = distribution.log_prob(sampled).numpy()
    return actions, log_probs


def update(actor, critic, actor_optimizer, critic_optimizer, rollout, args, rng, train_actor=True):
    maps, states = rollout["map"], rollout["state"]
    size = len(maps)
    mask = rollout["mask"]
    advantage = np.repeat(rollout["advantage"][:, None], 2, axis=1)
    selected = advantage[mask.astype(bool)]
    if len(selected) > 1:
        advantage = (advantage - selected.mean()) / (selected.std() + 1e-8)
    stats = []
    for _ in range(args.epochs):
        for indices in np.array_split(rng.permutation(size), max(1, int(np.ceil(size / args.batch_worlds)))):
            obs = {"map": torch.from_numpy(maps[indices]).flatten(0, 1),
                   "state": torch.from_numpy(states[indices]).flatten(0, 1)}
            with torch.set_grad_enabled(train_actor):
                distribution = actor.distribution(obs)
                actions = torch.from_numpy(rollout["actions"][indices].reshape(-1))
                valid = torch.from_numpy(mask[indices].reshape(-1))
                loss, entropy, kl = actor_objective(
                    distribution.log_prob(actions), torch.from_numpy(rollout["log_probs"][indices].reshape(-1)),
                    distribution.entropy(), torch.from_numpy(advantage[indices].reshape(-1)),
                    valid, args.clip, args.entropy)
            team_obs = {"map": torch.from_numpy(maps[indices, 0]),
                        "state": torch.from_numpy(states[indices, 0])}
            values = critic(team_obs)
            value_loss = .5 * (values - torch.from_numpy(rollout["returns"][indices])).square().mean()
            if not torch.isfinite(loss + value_loss):
                raise RuntimeError("Non-finite cooperative PPO loss")
            actor_optimizer.zero_grad()
            critic_optimizer.zero_grad()
            (loss + value_loss if train_actor else value_loss).backward()
            if train_actor:
                nn.utils.clip_grad_norm_(actor.parameters(), .5, error_if_nonfinite=True)
            nn.utils.clip_grad_norm_(critic.parameters(), .5, error_if_nonfinite=True)
            if train_actor:
                actor_optimizer.step()
            critic_optimizer.step()
            stats.append([loss.item(), value_loss.item(), entropy.item(), kl.item()])
            if train_actor and kl.item() > args.target_kl:
                return np.mean(stats, axis=0).tolist()
    return np.mean(stats, axis=0).tolist()


def initialize_models(seed, parent=None, coop_parent=None):
    if parent and coop_parent:
        raise ValueError("Choose a single-player or cooperative parent, not both")
    torch.manual_seed(seed)
    actor, critic = CoopActor(), TeamCritic()
    if parent:
        from stable_baselines3 import PPO
        actor.initialize_from_single(PPO.load(parent, device="cpu"))
    elif coop_parent:
        actor.load_state_dict(load_actor(coop_parent).state_dict())
    # SB3's loader calls set_random_seed with the OLD checkpoint's seed.
    # Native worlds and minibatches use a separate local NumPy generator;
    # explicitly restore the requested seed for all on-policy action sampling.
    torch.manual_seed(seed)
    return actor, critic


def train(args):
    torch.set_num_threads(1)
    rng = np.random.default_rng(args.seed)
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / "build"):
        raise ValueError("Training outputs must be under build/")
    output.mkdir(parents=True, exist_ok=False)
    actor, critic = initialize_models(args.seed, args.parent, args.coop_parent)
    partners = EpisodePartners(args.envs, args.historical, args.partner_mix, args.seed)
    # Frozen actor construction also uses Torch's global RNG. Keep the current
    # actor sampling stream independent of the number of loaded partners.
    torch.manual_seed(args.seed)
    deck = StageDeck(args.stages, args.seed) if args.balanced_stages else None
    actor_optimizer = torch.optim.Adam(actor.parameters(), lr=args.lr, eps=1e-5)
    critic_optimizer = torch.optim.Adam(critic.parameters(), lr=args.critic_lr, eps=1e-5)
    config = {k: str(v) if isinstance(v, Path) else v for k, v in vars(args).items()}
    config["historical"] = [str(path) for path in args.historical]
    parent_path = args.parent or args.coop_parent
    config.update({"started_utc": datetime.now(timezone.utc).isoformat(),
                   "format": "tanks3d-coop-v1", "actor_parameters": sum(p.numel() for p in actor.parameters()),
                   "sampling_seed": torch.initial_seed(),
                   "initial_sampling_rng_sha256": hashlib.sha256(torch.get_rng_state().numpy().tobytes()).hexdigest(),
                   "initial_critic_sha256": actor_hash(critic),
                   "native_sha256": hashlib.sha256((ROOT / "build/ai/libtanks3d_training.dylib").read_bytes()).hexdigest(),
                   "parent_sha256": hashlib.sha256(parent_path.read_bytes()).hexdigest() if parent_path else None,
                   "initialization": "Actor weights only; fresh team critic and optimizers, not exact optimizer continuation",
                   "historical_sha256": partners.file_hashes,
                   "sources": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in sorted((ROOT / "training").glob("*.py"))},
                   "reward": "symmetric native event ledger; no extra death or stationary penalty",
                   "time_limit": "artificial truncation bootstraps final pre-reset state; GAE stops at reset",
                   "actor_loss": "one shared actor; mean over ready slots; original proposal log probabilities",
                   "critic_loss": "one canonical P1-perspective team state per world transition",
                   "partner_training": "episode-fixed self/history/defender mixture; frozen slots excluded from actor loss; learning slot randomized",
                   "stage_sampling": "shuffled complete stage deck" if deck else "original independent stage sampling"})
    (output / "config.json").write_text(json.dumps(config, indent=2))
    initial_hash = actor_hash(actor)
    save_actor(output / "initial-actor.pt", actor, world_steps=0, parent=config["parent_sha256"])
    steps = proposals = valid_samples = native_ticks = episodes = 0
    ready_samples = actor_updates = 0
    kind_steps = dict.fromkeys(("self", "historical", "defender"), 0)
    started = time.monotonic()
    with ExitStack() as stack:
        envs = [stack.enter_context(CoopEnv(stages=args.stages, seed_limit=60000)) for _ in range(args.envs)]
        logs = stack.enter_context((output / "episodes.jsonl").open("w", buffering=1))
        starts = stack.enter_context((output / "episode-starts.jsonl").open("w", buffering=1))
        updates = stack.enter_context((output / "updates.jsonl").open("w", buffering=1))
        observations = []
        returns = np.zeros(args.envs)
        last_ticks = np.zeros(args.envs, dtype=int)
        def reset_world(i):
            seed = int(rng.integers(60000))
            obs, info = envs[i].reset(seed=seed, options={"stage": deck.next()} if deck else None)
            partners.reset(i, seed)
            starts.write(json.dumps({"world": i, "seed": seed, "stage": info["stage"],
                                    **partners.description(i)}) + "\n")
            return obs

        for i in range(args.envs):
            observations.append(reset_world(i))
        while steps < args.world_steps:
            length = min(args.rollout, (args.world_steps - steps) // args.envs)
            train_actor = steps >= args.critic_warmup_steps
            if not train_actor:
                length = min(length, (args.critic_warmup_steps - steps) // args.envs)
            maps = np.empty((length, args.envs, *observations[0]["map"].shape), np.float32)
            states = np.empty((length, args.envs, *observations[0]["state"].shape), np.float32)
            actions = np.empty((length, args.envs, 2), np.int64)
            log_probs = np.empty((length, args.envs, 2), np.float32)
            mask = np.empty_like(log_probs)
            rewards, values, next_values, terminal, boundary = [np.zeros((length, args.envs), np.float32) for _ in range(5)]
            for t in range(length):
                batch = stack_observations(observations)
                maps[t], states[t] = batch["map"], batch["state"]
                learning = partners.learner_mask()
                actions[t], log_probs[t] = propose_actions(actor, batch, learning)
                with torch.no_grad():
                    values[t] = critic(tensor_observation({k: v[:, 0] for k, v in batch.items()})).numpy()
                joint = partners.fill(batch, actions[t])
                executed, ready = execute_actions(batch, joint, not args.no_fire_guard)
                mask[t] = ready & learning
                ready_samples += int(mask[t].sum())
                valid_samples += int(mask[t].sum()) if train_actor else 0
                proposals += int(learning.sum())
                next_observations, ended = [], []
                for i, env in enumerate(envs):
                    kind_steps[partners.kinds[i]] += 1
                    obs, reward, term, trunc, info = env.step(executed[i])
                    next_observations.append(obs)
                    rewards[t, i], terminal[t, i], boundary[t, i] = reward * args.reward_scale, term, term or trunc
                    returns[i] += reward
                    native_ticks += int(info["ticks"]) - int(last_ticks[i])
                    last_ticks[i] = int(info["ticks"])
                    if term or trunc:
                        episodes += 1
                        logs.write(json.dumps({"world_steps": steps + (t + 1) * args.envs,
                                               "episode": episodes, "return": returns[i],
                                               "digest": env.digest(), **info,
                                               **partners.description(i)}) + "\n")
                        ended.append(i)
                with torch.no_grad():
                    next_batch = stack_observations(next_observations)
                    next_values[t] = critic(tensor_observation({k: v[:, 0] for k, v in next_batch.items()})).numpy()
                for i in ended:
                    next_observations[i] = reset_world(i)
                    returns[i], last_ticks[i] = 0, 0
                observations = next_observations
            advantage, targets = team_gae(rewards, values, next_values, terminal, boundary, args.gamma, args.gae)
            flat = lambda x: x.reshape(length * args.envs, *x.shape[2:])
            metrics = update(actor, critic, actor_optimizer, critic_optimizer, {
                "map": flat(maps), "state": flat(states), "actions": flat(actions),
                "log_probs": flat(log_probs), "mask": flat(mask),
                "advantage": advantage.flatten(), "returns": targets.flatten()}, args, rng, train_actor)
            actor_updates += int(train_actor)
            steps += length * args.envs
            row = {"world_steps": steps, "actor_proposals": proposals, "valid_actor_samples": valid_samples,
                   "ready_learner_samples": ready_samples, "actor_updates": actor_updates,
                   "actor_update_enabled": train_actor, "partner_world_steps": kind_steps.copy(),
                   **partners.totals,
                   "native_ticks": native_ticks, "episodes": episodes, "loss": metrics,
                   "world_steps_per_second": steps / (time.monotonic() - started)}
            updates.write(json.dumps(row) + "\n")
            if steps % args.checkpoint_steps == 0 or steps == args.world_steps:
                save_actor(output / f"actor-{steps}.pt", actor, world_steps=steps, seed=args.seed)
                print(json.dumps(row), flush=True)
        save_actor(output / "actor.pt", actor, world_steps=steps, seed=args.seed, parent=config["parent_sha256"])
        torch.save({"actor": actor.state_dict(), "critic": critic.state_dict(),
                    "actor_optimizer": actor_optimizer.state_dict(), "critic_optimizer": critic_optimizer.state_dict(),
                    "world_steps": steps, "torch_rng": torch.get_rng_state(),
                    "numpy_rng": rng.bit_generator.state, "config": config,
                    "partner_rng": partners.rng.bit_generator.state,
                    "stage_deck_rng": deck.rng.bit_generator.state if deck else None,
                    "remaining_stages": deck.remaining if deck else None,
                    "resume_contract": "Optimizer/weights saved; native live worlds are not serialized. Continue in new episodes."},
                   output / "training-state.pt")
        final = {**row, "finished_utc": datetime.now(timezone.utc).isoformat(),
                 "initial_actor_hash": initial_hash, "final_actor_hash": actor_hash(actor),
                 "cooperative_weight_norm": actor.cooperative.weight.detach().norm().item(),
                 "all_parameters_finite": all(torch.isfinite(p).all().item() for p in actor.parameters())}
        partners.verify_frozen()
        final["frozen_partners_unchanged"] = True
        if ((args.critic_warmup_steps < args.world_steps and final["final_actor_hash"] == initial_hash)
                or not final["all_parameters_finite"]):
            raise RuntimeError("Cooperative actor did not update finitely")
        (output / "complete.json").write_text(json.dumps(final, indent=2))
        print(json.dumps(final), flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--output", type=Path, required=True)
    source = p.add_mutually_exclusive_group()
    source.add_argument("--parent", type=Path)
    source.add_argument("--coop-parent", type=Path)
    p.add_argument("--historical", type=Path, nargs="*", default=[])
    p.add_argument("--partner-mix", type=float, nargs=3, default=[1., 0., 0.],
                   metavar=("SELF", "HISTORY", "DEFENDER"))
    p.add_argument("--balanced-stages", action="store_true")
    p.add_argument("--critic-warmup-steps", type=int, default=0)
    p.add_argument("--seed", type=int, default=14001)
    p.add_argument("--world-steps", type=int, default=262144)
    p.add_argument("--envs", type=int, default=4)
    p.add_argument("--rollout", type=int, default=256)
    p.add_argument("--batch-worlds", type=int, default=128)
    p.add_argument("--epochs", type=int, default=4)
    p.add_argument("--lr", type=float, default=3e-5)
    p.add_argument("--critic-lr", type=float, default=3e-4)
    p.add_argument("--gamma", type=float, default=.9995)
    p.add_argument("--gae", type=float, default=.98)
    p.add_argument("--clip", type=float, default=.1)
    p.add_argument("--target-kl", type=float, default=.03)
    p.add_argument("--entropy", type=float, default=.005)
    p.add_argument("--reward-scale", type=float, default=.1)
    p.add_argument("--checkpoint-steps", type=int, default=65536)
    p.add_argument("--stages", type=int, nargs="+", default=[1])
    p.add_argument("--no-fire-guard", action="store_true")
    args = p.parse_args()
    if (min(args.envs, args.world_steps, args.rollout, args.batch_worlds, args.epochs, args.checkpoint_steps) < 1
            or args.world_steps % args.envs or not 0 < args.gamma <= 1 or not 0 < args.gae <= 1
            or not 0 < args.clip < 1 or min(args.lr, args.critic_lr, args.target_kl, args.reward_scale) <= 0
            or args.entropy < 0 or any(not 1 <= stage <= 35 for stage in args.stages)):
        p.error("Invalid cooperative training configuration")
    if (args.seed < 0 or not 0 <= args.critic_warmup_steps <= args.world_steps
            or args.critic_warmup_steps % args.envs or len(set(args.stages)) != len(args.stages)):
        p.error("Invalid seed, warmup boundary or duplicate stages")
    train(args)


if __name__ == "__main__":
    main()
