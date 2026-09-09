"""Capture publication previews from the unchanged, attested Alpha 5 app."""

import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
tag = "v0.1.0-alpha.5"
candidate = root / "build/release" / tag
evidence = Path(__file__).resolve().parent
attestation = dict(line.split("=", 1) for line in
                   (candidate / "attestation.txt").read_text().splitlines())
archive = candidate / attestation["artifact_filename"]
assert hashlib.sha256(archive.read_bytes()).hexdigest() == attestation["artifact_sha256"]
capture = evidence / "candidate-capture"
shots = evidence / "screenshots"
capture.mkdir(mode=0o700)
shots.mkdir(mode=0o700)
subprocess.run(["/usr/bin/ditto", "-x", "-k", str(archive), str(capture)], check=True)
app = capture / "Tanks3D.app"
binary = app / "Contents/MacOS/Tanks3D"
subprocess.run(["/usr/bin/codesign", "--verify", "--deep", "--strict", str(app)], check=True)
probe = subprocess.check_output([str(binary), "--self-test=release-performance-capabilities"])
capabilities = json.loads(probe)
assert capabilities["source_commit"] == attestation["source_commit"]
assert capabilities["source_tag"] == attestation["source_tag"]
(evidence / "screenshot-candidate-capabilities.json").write_bytes(probe)
plan = [
    ("one-player", 600, ["--stage=1", "--quick-start"]),
    ("two-player", 600, ["--stage=1", "--quick-start-2p"]),
    ("base-usa", 2, ["--stage=1", "--camera-elevation=60", "--quick-start", "--base-damage-showcase"]),
    ("base-ussr", 2, ["--stage=1", "--camera-elevation=60", "--quick-start-ussr", "--base-damage-showcase"]),
    ("base-germany", 2, ["--stage=1", "--camera-elevation=60", "--quick-start-germany", "--base-damage-showcase"]),
    ("bonuses", 2, ["--stage=1", "--quick-start", "--bonus-showcase"]),
    ("settlement", 2, ["--stage=1", "--quick-start-2p", "--settlement-showcase"]),
]
records = []
for name, frame, arguments in plan:
    command = [str(binary), *arguments,
               "--release-screenshot=" + str(shots / (name + ".png")),
               "--release-screenshot-frame=" + str(frame)]
    print("Capturing " + name, flush=True)
    with (evidence / ("capture-" + name + ".stdout")).open("xb") as stdout, \
         (evidence / ("capture-" + name + ".stderr")).open("xb") as stderr:
        subprocess.run(command, stdout=stdout, stderr=stderr, check=True, timeout=90)
    records.append({"name": name, "command": command,
                    "sha256": hashlib.sha256((shots / (name + ".png")).read_bytes()).hexdigest()})
(evidence / "screenshot-capture-record.json").write_text(json.dumps({
    "candidate_sha256": attestation["artifact_sha256"],
    "source_commit": attestation["source_commit"],
    "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
    "captures": records,
    "scope": "Publication previews; bases, bonuses and settlement use staged showcases. No human QA result."
}, indent=2) + "\n")
