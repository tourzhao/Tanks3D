"""Watch or record the real two-player game driven by a shared cooperative actor."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import imageio_ffmpeg
import torch

from training.env import ROOT
from training.coop_env import CoopEnv
from training.coop_policy import load_actor
from training.coop_control import CoopController


def main():
    p = argparse.ArgumentParser(description=__doc__)
    source = p.add_mutually_exclusive_group(required=True)
    source.add_argument("--model", type=Path)
    source.add_argument("--replay", type=Path)
    source.add_argument("--rule-p2", choices=("defender", "tactical"))
    p.add_argument("--partner", choices=("self", "initial", "defender", "tactical"), default="self")
    p.add_argument("--initial", type=Path)
    p.add_argument("--seed", type=int, default=1900000)
    p.add_argument("--stage", type=int, default=1)
    p.add_argument("--seconds", type=int, default=120)
    p.add_argument("--trace", type=Path, required=True)
    p.add_argument("--record", type=Path)
    p.add_argument("--hidden", action="store_true")
    args = p.parse_args()
    for path in (args.record, args.trace):
        if path is not None:
            if not path.resolve().is_relative_to(ROOT / "build") or path.exists():
                p.error("Choose a fresh output under build/")
            path.parent.mkdir(parents=True, exist_ok=True)
    replay = json.loads(args.replay.read_text()) if args.replay else None
    if replay:
        args.seed, args.stage, args.seconds = replay["seed"], replay["stage"], replay["max_seconds"]
        args.partner, args.rule_p2 = replay["partner"], replay.get("rule_p2")
    if not 1 <= args.stage <= 35 or not 1 <= args.seconds <= 3600 or not 0 <= args.seed < 2**32:
        p.error("Invalid episode settings")
    if args.partner == "initial" and not args.initial and not replay:
        p.error("Initial P1 actor required")
    torch.set_num_threads(1)
    controller = None if replay else CoopController(load_actor(args.model) if args.model else None,
        args.partner, load_actor(args.initial) if args.partner == "initial" else None,
        rule_p2=args.rule_p2 or "defender")
    if controller:
        controller.reset(args.seed)
    writer = None
    if args.record:
        writer = imageio_ffmpeg.write_frames(str(args.record), (960, 540), fps=20, codec="libx264",
            pix_fmt_in="rgb24", pix_fmt_out="yuv420p", quality=7, macro_block_size=2, ffmpeg_log_level="warning")
        writer.send(None)
    actions = []
    try:
        with CoopEnv(max_seconds=args.seconds, render_mode="rgb_array" if args.hidden else "human") as env:
            obs, info = env.reset(seed=args.seed, options={"stage": args.stage})
            limit = len(replay["actions"]) if replay else args.seconds * 20
            for decision in range(limit):
                started = time.monotonic()
                action = replay["actions"][decision] if replay else controller.predict(obs).tolist()
                obs, _, terminated, truncated, info = env.step(action)
                actions.append(action)
                before_render = env.digest()
                frame = env.render()
                if writer:
                    # Some macOS back buffers return the previous draw; capture
                    # a second presentation at identical simulation state.
                    frame = env.render()
                    writer.send(frame)
                else:
                    time.sleep(max(0, .05 - (time.monotonic() - started)))
                if env.digest() != before_render:
                    raise RuntimeError("Rendering mutated native gameplay")
                if terminated or truncated:
                    break
            if replay and (env.digest() != replay["digest"] or actions != replay["actions"]):
                raise RuntimeError("Cooperative replay diverged")
            sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest() if path else None
            trace = {"seed": args.seed, "stage": args.stage, "max_seconds": args.seconds,
                     "lives_per_player": 3, "actions": actions, "digest": env.digest(), "info": info,
                     "native_sha256": sha(ROOT / "build/ai/libtanks3d_training.dylib"),
                     "model_sha256": sha(args.model), "initial_sha256": sha(args.initial),
                     "rule_p2": args.rule_p2,
                     "rule_policy_sha256": sha(ROOT / "training/rule_policy.py"),
                     "legacy_defender_sha256": sha(ROOT / "training/bot.py"),
                     "partner": args.partner, "controller": controller.counts if controller else None,
                     "replay_source": str(args.replay) if args.replay else None,
                     "stochastic": bool(args.model or args.partner == "initial"),
                     "fire_guard": True, "fps": 20, "size": [960, 540],
                     "capture_double_draw_same_state": writer is not None}
            args.trace.write_text(json.dumps(trace, indent=2))
            print(json.dumps({"frames": len(actions), "digest": env.digest(), **info}), flush=True)
    finally:
        if writer:
            writer.close()


if __name__ == "__main__":
    main()
