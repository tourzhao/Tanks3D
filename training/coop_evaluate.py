"""Evaluate frozen P2 actors with paired seeds and explicit P1 partner identities."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import torch

from training.env import ROOT
from training.coop_env import CoopEnv
from training.coop_policy import load_actor
from training.coop_control import CoopController


class MovementObserver:
    """Descriptive moving-command stalls; no inference of obstruction or reward cause."""
    def __init__(self):
        self.current = np.zeros(2)
        self.near = np.zeros(2)
        self.total = np.zeros(2)
        self.maximum = np.zeros(2)
        self.near_total = np.zeros(2)
        self.separations = []

    def flush(self, slot):
        if self.current[slot] >= 1 - 1e-8:
            self.total[slot] += self.current[slot]
            self.maximum[slot] = max(self.maximum[slot], self.current[slot])
            self.near_total[slot] += self.near[slot]
        self.current[slot] = self.near[slot] = 0

    def step(self, before, after, actions, seconds):
        delta = np.linalg.norm((after["state"][:, :2] - before["state"][:, :2]) * 26, axis=1)
        distance = float(np.linalg.norm((after["state"][0, :2] - after["state"][1, :2]) * 26))
        both_ready = bool((after["state"][:, 332] > .5).all())
        if both_ready:
            self.separations.append(distance)
        for slot in range(2):
            stationary = (before["state"][slot, 332] > .5 and after["state"][slot, 332] > .5
                          and actions[slot] // 2 > 0 and delta[slot] < .01)
            if stationary:
                self.current[slot] += seconds
                if both_ready and distance <= 2.5:
                    self.near[slot] += seconds
            else:
                self.flush(slot)

    def finish(self):
        for slot in range(2):
            self.flush(slot)
        return {**{f"p{slot + 1}_{key}": float(values[slot]) for slot in range(2)
                   for key, values in (("stationary_seconds", self.total),
                                       ("max_stationary_seconds", self.maximum),
                                       ("stationary_near_ally_seconds", self.near_total))},
                "mean_ready_separation_tiles": float(np.mean(self.separations)) if self.separations else 0,
                "max_ready_separation_tiles": max(self.separations, default=0)}


def evaluate(model, partner, initial, first_seed, episodes, stages=(1,), max_seconds=120,
             fire_guard=True, progress=False, rule_p2="defender"):
    actor = load_actor(model) if model else None
    initial_actor = load_actor(initial) if partner == "initial" else None
    controller = CoopController(actor, partner, initial_actor, fire_guard, rule_p2)
    rows = []
    with CoopEnv(max_seconds=max_seconds) as env:
        for index in range(episodes):
            seed, stage = first_seed + index, stages[index % len(stages)]
            obs, info = env.reset(seed=seed, options={"stage": stage})
            controller.reset(seed)
            observer = MovementObserver()
            total = 0
            while True:
                actions = controller.predict(obs)
                before_ticks = info["ticks"]
                following, reward, terminal, truncated, info = env.step(actions)
                observer.step(obs, following, actions, (info["ticks"] - before_ticks) / 60)
                total += reward
                obs = following
                if terminal or truncated:
                    break
            rows.append({**info, "seed": seed, "stage": stage, "return": total,
                         "digest": env.digest(), "movement": observer.finish(),
                         "controller": controller.counts.copy()})
            if progress and (index + 1) % 10 == 0:
                print(json.dumps({"episodes": index + 1, "clears": int(sum(r["won"] for r in rows))}), flush=True)
    keys = [k for k in rows[0] if k not in ("seed", "episode_seed", "stage", "digest", "movement", "controller")]
    return {"summary": {k: float(np.mean([row[k] for row in rows])) for k in keys},
            "movement_summary": {k: float(np.mean([row["movement"][k] for row in rows])) for k in rows[0]["movement"]},
            "episodes": rows}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    source = p.add_mutually_exclusive_group(required=True)
    source.add_argument("--model", type=Path)
    source.add_argument("--defender-p2", action="store_true")
    source.add_argument("--rule-p2", choices=("defender", "tactical"))
    p.add_argument("--partner", choices=("self", "initial", "defender", "tactical"), default="self")
    p.add_argument("--initial", type=Path)
    p.add_argument("--seed", type=int, required=True)
    p.add_argument("--episodes", type=int, default=100)
    p.add_argument("--stages", nargs="+", type=int, default=[1])
    p.add_argument("--max-seconds", type=float, default=120)
    p.add_argument("--no-fire-guard", action="store_true")
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    if (args.episodes < 1 or args.seed < 0 or args.seed + args.episodes > 2**32
            or not args.stages or any(not 1 <= s <= 35 for s in args.stages)
            or not args.output.resolve().is_relative_to(ROOT / "build") or args.output.exists()
            or (args.partner == "initial" and args.initial is None)):
        p.error("Invalid seeds, stages, partner or fresh build/ output path")
    torch.set_num_threads(1)
    result = evaluate(args.model, args.partner, args.initial, args.seed, args.episodes,
                      tuple(args.stages), args.max_seconds, not args.no_fire_guard, progress=True,
                      rule_p2=args.rule_p2 or "defender")
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest() if path else None
    result.update({"model": str(args.model), "model_sha256": sha(args.model),
                   "partner": args.partner, "initial_sha256": sha(args.initial),
                   "native_sha256": sha(ROOT / "build/ai/libtanks3d_training.dylib"),
                   "evaluator_sha256": sha(Path(__file__)),
                   "controller_sha256": sha(ROOT / "training/coop_control.py"),
                   "rule_p2": None if args.model else args.rule_p2 or "defender",
                   "rule_policy_sha256": sha(ROOT / "training/rule_policy.py"),
                   "legacy_defender_sha256": sha(ROOT / "training/bot.py"),
                   "fire_guard": not args.no_fire_guard,
                   "stochastic": bool(args.model or args.partner == "initial"),
                   "max_seconds": args.max_seconds, "lives_per_player": 3,
                   "scope": "Real two-player independent stages; script partners are not human acceptance."})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2))
    print(json.dumps(result["summary"]), flush=True)


if __name__ == "__main__":
    main()
