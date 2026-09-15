"""Evaluate trained/random/scripted policies on an explicitly separate seed set."""

import argparse
import hashlib
import json
from pathlib import Path
import time
import numpy as np
import torch
from stable_baselines3 import PPO
from training.env import TanksEnv, ROOT
from training.bot import Defender
from training.hybrid import HybridPolicy, HYBRID_MODES, COUNTERS


def main():
    p = argparse.ArgumentParser()
    p.add_argument(
        "--policy", choices=["ppo", "hybrid", "random", "defender", "north"], required=True
    )
    p.add_argument("--hybrid-mode", choices=HYBRID_MODES)
    p.add_argument("--model", type=Path)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--episodes", type=int, default=100)
    p.add_argument("--seed-start", type=int, default=100000)
    p.add_argument("--stages", type=int, nargs="+", default=[1])
    p.add_argument("--max-seconds", type=int, default=120)
    p.add_argument(
        "--stochastic",
        action="store_true",
        help="Sample the policy with a reproducible per-episode seed",
    )
    args = p.parse_args()
    if args.episodes < 1 or not 0 <= args.seed_start < args.seed_start + args.episodes <= 2**32:
        p.error("Positive episode count and uint32 episode seeds are required")
    if any(not 1 <= stage <= 35 for stage in args.stages):
        p.error("Stages must be in [1,35]")
    if not args.output.resolve().is_relative_to((ROOT / "build").resolve()):
        p.error("Outputs must be under build/")
    if args.output.exists():
        p.error("Output exists; use a new filename")
    if args.policy in ("ppo", "hybrid") and args.model is None:
        p.error("--model is required for PPO or hybrid")
    if args.hybrid_mode is not None and args.policy != "hybrid":
        p.error("--hybrid-mode requires --policy hybrid")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    torch.set_num_threads(1)
    model = PPO.load(args.model, device="cpu") if args.policy in ("ppo", "hybrid") else None
    hybrid = (
        HybridPolicy(model, args.hybrid_mode or "defense") if args.policy == "hybrid" else None
    )
    bot = Defender()
    episodes = []
    started = time.monotonic()
    with TanksEnv(max_seconds=args.max_seconds) as env:
        for i in range(args.episodes):
            seed = args.seed_start + i
            stage = args.stages[i % len(args.stages)]
            obs, info = env.reset(seed=seed, options={"stage": stage})
            rng = np.random.default_rng(seed + 2**32)
            torch.manual_seed(seed + 123456789)
            bot.reset()
            if hybrid:
                hybrid.reset()
            total = 0
            repeat_action = 0
            decision = 0
            while True:
                if hybrid:
                    action = hybrid.predict(obs, deterministic=not args.stochastic)
                elif model is not None:
                    action = int(model.predict(obs, deterministic=not args.stochastic)[0])
                elif args.policy == "defender":
                    action = bot.predict(obs)
                elif args.policy == "north":
                    action = 3
                else:
                    # Persist random motion for 0.5 s, avoiding a deliberately
                    # weak baseline that only jitters every decision frame.
                    if decision % 10 == 0:
                        repeat_action = int(rng.integers(10))
                    action = repeat_action
                obs, reward, terminated, truncated, info = env.step(action)
                decision += 1
                total += reward
                if terminated or truncated:
                    break
            episodes.append({"seed": seed, **info, "return": total, "digest": env.digest()})
            if hybrid:
                episodes[-1]["hybrid"] = hybrid.statistics()
            if (i + 1) % 10 == 0:
                print(f"{args.policy}: {i+1}/{args.episodes} episodes", flush=True)
    summary = {
        key: float(np.mean([e[key] for e in episodes]))
        for key in (
            "won",
            "base_alive",
            "kills",
            "deaths",
            "own_base_hits",
            "return",
            "truncated",
            "seconds",
        )
    }
    result = {
        "policy": args.policy,
        "stochastic": args.stochastic,
        "episodes": episodes,
        "summary": summary,
        "elapsed_seconds": time.monotonic() - started,
        "model_sha256": (
            hashlib.sha256(args.model.read_bytes()).hexdigest() if args.model else None
        ),
        "native_sha256": hashlib.sha256(
            (ROOT / "build/ai/libtanks3d_training.dylib").read_bytes()
        ).hexdigest(),
        "stages": args.stages,
        "seed_start": args.seed_start,
        "max_seconds": args.max_seconds,
        "lives": 3,
    }
    if hybrid:
        totals = {key: sum(e["hybrid"][key] for e in episodes) for key in COUNTERS}
        result["hybrid"] = {
            "configuration": hybrid.configuration(),
            **totals,
            "rule_fraction": totals["rule_decisions"] / totals["decisions"],
            "defender_fraction": totals["defender_calls"] / totals["decisions"],
            "fire_suppression_fraction": totals["fire_suppressions"] / totals["decisions"],
            "source_sha256": {
                name: hashlib.sha256((ROOT / "training" / name).read_bytes()).hexdigest()
                for name in ("hybrid.py", "bot.py", "evaluate.py", "policy.py", "env.py")
            },
        }
    args.output.write_text(json.dumps(result, indent=2))
    print(json.dumps(summary), flush=True)


if __name__ == "__main__":
    main()
