"""Watch a trained policy, record native 3D frames, or replay recorded actions."""

import argparse
import hashlib
import json
from pathlib import Path
import time

import torch
from stable_baselines3 import PPO
import imageio_ffmpeg

from training.env import TanksEnv, ROOT
from training.bot import Defender
from training.hybrid import HybridPolicy, HYBRID_MODES


def main():
    p = argparse.ArgumentParser()
    source = p.add_mutually_exclusive_group(required=True)
    source.add_argument("--model", type=Path)
    source.add_argument("--replay", type=Path)
    source.add_argument("--defender", action="store_true")
    p.add_argument("--hybrid-mode", choices=HYBRID_MODES)
    p.add_argument("--seed", type=int, default=70007)
    p.add_argument("--stage", type=int, default=1)
    p.add_argument("--seconds", type=int, default=30)
    p.add_argument("--record", type=Path)
    p.add_argument("--trace", type=Path, required=True)
    p.add_argument("--hidden", action="store_true")
    p.add_argument(
        "--stochastic",
        action="store_true",
        help="Sample the policy with a reproducible per-episode seed",
    )
    args = p.parse_args()
    if args.seconds < 1:
        p.error("Positive playback duration required")
    if args.hybrid_mode is not None and args.model is None:
        p.error("--hybrid-mode requires --model")
    for path in (args.record, args.trace):
        if path is not None:
            if not path.resolve().is_relative_to((ROOT / "build").resolve()):
                p.error("Outputs must be under build/")
            if path.exists():
                p.error(f"Output exists: {path}")
            path.parent.mkdir(parents=True, exist_ok=True)
    replay = json.loads(args.replay.read_text()) if args.replay else None
    if replay:
        args.seed, args.stage = replay["seed"], replay["stage"]
        args.stochastic = replay.get("stochastic", False)
    torch.set_num_threads(1)
    model = PPO.load(args.model, device="cpu") if args.model else None
    hybrid = HybridPolicy(model, args.hybrid_mode) if args.hybrid_mode else None
    bot = Defender()
    torch.manual_seed(args.seed + 123456789)
    actions = []
    writer = None
    viewer_closed = False
    try:
        if args.record:
            writer = imageio_ffmpeg.write_frames(
                str(args.record),
                (960, 540),
                fps=20,
                codec="libx264",
                pix_fmt_in="rgb24",
                pix_fmt_out="yuv420p",
                macro_block_size=2,
                quality=7,
                ffmpeg_log_level="warning",
            )
            writer.send(None)
        with TanksEnv(render_mode="rgb_array" if args.hidden else "human") as env:
            obs, info = env.reset(seed=args.seed, options={"stage": args.stage})
            limit = len(replay["actions"]) if replay else args.seconds * 20
            for decision in range(limit):
                started = time.monotonic()
                if replay:
                    action = int(replay["actions"][decision])
                elif hybrid:
                    action = hybrid.predict(obs, deterministic=not args.stochastic)
                elif model is not None:
                    action = int(model.predict(obs, deterministic=not args.stochastic)[0])
                else:
                    action = bot.predict(obs)
                obs, _, terminated, truncated, info = env.step(action)
                actions.append(action)
                try:
                    frame = env.render()
                except RuntimeError as error:
                    if str(error) == "Viewer closed":
                        viewer_closed = True
                        break
                    raise
                if writer:
                    writer.send(frame)
                else:
                    time.sleep(max(0, 0.05 - (time.monotonic() - started)))
                if terminated or truncated:
                    break
            digest = env.digest()
            if replay and not viewer_closed and digest != replay["digest"]:
                raise RuntimeError("Replay diverged from its recorded gameplay digest")
            args.trace.write_text(
                json.dumps(
                    {
                        "seed": args.seed,
                        "stage": args.stage,
                        "source": (
                            "replay"
                            if replay
                            else (
                                "hybrid"
                                if hybrid
                                else "ppo" if model is not None else "defender"
                            )
                        ),
                        "hybrid": (
                            {
                                "configuration": hybrid.configuration(),
                                **hybrid.statistics(),
                                "source_sha256": hashlib.sha256(
                                    (ROOT / "training/hybrid.py").read_bytes()
                                ).hexdigest(),
                            }
                            if hybrid
                            else None
                        ),
                        "replay_source": str(args.replay) if replay else None,
                        "actions": actions,
                        "digest": digest,
                        "info": info,
                        "fps": 20,
                        "viewer_closed": viewer_closed,
                        "stochastic": args.stochastic,
                        "lives": 3,
                        "native_sha256": hashlib.sha256(
                            (ROOT / "build/ai/libtanks3d_training.dylib").read_bytes()
                        ).hexdigest(),
                        "model_sha256": (
                            hashlib.sha256(args.model.read_bytes()).hexdigest()
                            if args.model
                            else None
                        ),
                    },
                    indent=2,
                )
            )
            print(json.dumps({"decisions": len(actions), "digest": digest, **info}), flush=True)
    finally:
        if writer:
            writer.close()


if __name__ == "__main__":
    main()
