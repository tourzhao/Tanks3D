#!/usr/bin/env python3
"""Exercise a real TCP LAN room from two independent headless Godot processes.

This verifies GDScript -> GDExtension -> production protocol integration. It is
loopback QA on one Mac, not a physical two-machine/Wi-Fi or graphical test.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "scripts"))
from test_godot_import import validate_output


def validate_pair(reports, traces, target_ticks=480):
    if reports["host"]["pid"] == reports["guest"]["pid"]:
        raise RuntimeError("LAN peers must be independent Godot processes")
    for role, report in reports.items():
        if (report.get("status") != "passed" or report.get("role") != role or
                report.get("last_tick", 0) < target_ticks or not report.get("disconnected") or
                not report.get("disconnect_reason") or report.get("players_moved") != [True, True] or
                len(report.get("player_shots", [])) != 2 or min(report["player_shots"]) < 1 or
                report.get("local_player") != (0 if role == "host" else 1) or
                report.get("input_mask") != (17 if role == "host" else 24)):
            raise RuntimeError(f"{role} did not prove settings, both player inputs, ticks and disconnect")
        expected = {"stage": 1, "players": 2, "ai_p2": False, "lives": 5,
                    "nation_p1": 2, "nation_p2": 1, "max_hp": 6, "enemy_speed": -25,
                    "enemy_fire": 10, "enemy_spawn": -30,
                    "camera_yaw": 25 if role == "host" else -35,
                    "camera_elevation": 60 if role == "host" else 45}
        if report.get("settings") != expected:
            raise RuntimeError(f"{role} did not confirm the expected negotiated and local settings")
    common = sorted(tick for tick in set(traces["host"]) & set(traces["guest"]) if tick > 0)
    if len(common) < 360 or not common or common[-1] < target_ticks:
        observed = {role: {"count": len(trace), "first": min(trace, default=-1),
                           "last": max(trace, default=-1)} for role, trace in traces.items()}
        raise RuntimeError("Fewer than 360 shared authoritative tick observations or final tick missing: "
                           f"matched={len(common)}, last_common={common[-1] if common else -1}, "
                           f"target={target_ticks}, peers={observed}")
    for tick in common:
        if not traces["host"][tick] or traces["host"][tick] != traces["guest"][tick]:
            raise RuntimeError(f"Independent Godot process state/RNG digests diverged at tick {tick}")
    return {"matched_ticks": len(common), "first_tick": common[0], "last_tick": common[-1],
            "last_digest": traces["host"][common[-1]]}


def load_trace(path):
    rows = [json.loads(line) for line in path.read_text().splitlines() if line]
    result = {}
    for row in rows:
        tick = row["tick"]
        if not isinstance(tick, int) or tick < 0 or tick in result:
            raise RuntimeError(f"Duplicate/invalid tick in {path}")
        result[tick] = row["digest"]
    return result


def run_pair(godot, project, output, ticks=480, timeout=45):
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise RuntimeError(f"LAN evidence directory must be empty: {output}")
    processes, streams, commands = {}, {}, {}
    try:
        for role in ["host", "guest"]:
            address = None
            if role == "guest":
                deadline = time.monotonic() + 10
                endpoint = output / "endpoint.json"
                while not endpoint.exists():
                    if processes["host"].poll() is not None or time.monotonic() >= deadline:
                        raise RuntimeError("Host failed to publish a TCP endpoint; inspect host logs")
                    time.sleep(0.02)
                address = json.loads(endpoint.read_text())["address"]
                if not address.startswith("127.0.0.1:"):
                    raise RuntimeError("Check endpoint must remain on loopback")
            command = [str(godot), "--headless", "--path", str(project),
                       "--log-file", str(output / f"{role}.engine.log"),
                       "--script", "res://lan_checks.gd", "--", f"--role={role}",
                       f"--output={output}", f"--ticks={ticks}"]
            if address:
                command.append(f"--address={address}")
            commands[role] = command
            streams[role] = (output / f"{role}.console.log").open("w")
            processes[role] = subprocess.Popen(command, cwd=ROOT, stdout=streams[role], stderr=subprocess.STDOUT)
        deadline = time.monotonic() + timeout
        for role, process in processes.items():
            process.wait(timeout=max(0.1, deadline - time.monotonic()))
            streams[role].close()
            text = (output / f"{role}.console.log").read_text(errors="replace")
            engine_log = output / f"{role}.engine.log"
            validate_output(text + "\n" + (engine_log.read_text(errors="replace") if engine_log.exists() else ""),
                            process.returncode, f"Godot LAN {role}")
            if text.count("TANKS_LAN_PROCESS_PASSED ") != 1:
                raise RuntimeError(f"{role} did not emit exactly one completion marker")
        reports = {role: json.loads((output / f"{role}.report.json").read_text()) for role in processes}
        traces = {role: load_trace(output / f"{role}.ticks.jsonl") for role in processes}
        match = validate_pair(reports, traces, ticks)
        extension = project / "native/libtanks_sample.dylib"
        receipt = {"status": "passed", "scope": "two independent headless Godot processes, real loopback TCP",
                   "checks": ["host-join", "settings", "role-input", "movement-fire", "state-rng-digests", "disconnect"],
                   "target_ticks": ticks, **match, "processes": reports, "commands": commands,
                   "native_sha256": hashlib.sha256(extension.read_bytes()).hexdigest(),
                   "limitations": ["One Mac loopback, not physical two-Mac/Wi-Fi QA", "No GPU or controller validation"]}
        (output / "result.json").write_text(json.dumps(receipt, indent=2) + "\n")
        print("TANKS_GODOT_LAN_PASSED " + json.dumps(match), flush=True)
        print(f"LAN receipt: {output / 'result.json'}", flush=True)
        return receipt
    finally:
        for process in processes.values():
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
        for stream in streams.values():
            stream.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", type=Path, default=ROOT / "build/godot-tools/Godot.app/Contents/MacOS/Godot")
    parser.add_argument("--project", type=Path, default=ROOT / "build/godot/project")
    parser.add_argument("--output", type=Path, help="Empty evidence directory; default creates a unique build/godot/lan-validation-* directory")
    parser.add_argument("--ticks", type=int, default=480)
    parser.add_argument("--timeout", type=int, default=45)
    args = parser.parse_args()
    project = args.project.resolve()
    if args.output is None:
        parent = ROOT / "build/godot"
        parent.mkdir(parents=True, exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix="lan-validation-", dir=parent))
    else:
        output = args.output.resolve()
    if not project.is_relative_to(ROOT / "build") or not output.is_relative_to(ROOT / "build"):
        parser.error("Project and output must be inside the repository build directory")
    if args.ticks < 360 or args.ticks > 900 or args.timeout < 15 or args.timeout > 120:
        parser.error("Use 360..900 ticks and a 15..120 second timeout")
    if not (project / "lan_checks.gd").is_file():
        parser.error("Stage the project (including lan_checks.gd) before running this check")
    run_pair(args.godot.resolve(), project, output, args.ticks, args.timeout)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error)) from error
