"""Export fresh homepage previews from the unchanged Alpha 5 candidate."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
output = Path(__file__).resolve().parent
candidate = root / "build/release/v0.1.0-alpha.5"
attestation = dict(line.split("=", 1) for line in
                   (candidate / "attestation.txt").read_text().splitlines())
archive = candidate / attestation["artifact_filename"]
assert hashlib.sha256(archive.read_bytes()).hexdigest() == attestation["artifact_sha256"]
app = root / "build/release-evidence/v0.1.0-alpha.5/candidate-capture/Tanks3D.app"
binary = app / "Contents/MacOS/Tanks3D"
prior = json.loads((root / "build/release-evidence/v0.1.0-alpha.5/screenshot-capture-record.json").read_text())
assert hashlib.sha256(binary.read_bytes()).hexdigest() == prior["binary_sha256"]
subprocess.run(["/usr/bin/codesign", "--verify", "--deep", "--strict", str(app)], check=True)

plan = [
    ("battlefield-coop", 600, ["--stage=26", "--camera-yaw=-25", "--camera-elevation=50", "--quick-start-2p"], "Two-player spawn on original stage 26, Pixel Style OFF."),
    ("forest-solo", 2, ["--stage=26", "--camera-yaw=-25", "--camera-elevation=50", "--quick-start", "--forest-cover-showcase"], "Staged forest-cover position on original stage 26, Pixel Style OFF."),
    ("national-enemies", 2, ["--stage=26", "--camera-yaw=-25", "--camera-elevation=50", "--quick-start-germany", "--tank-showcase"], "Built-in tank showcase arena: German player against American and Soviet enemy roles."),
    ("battle-report", 600, ["--stage=1", "--quick-start-2p", "--settlement-showcase"], "Built-in two-player settlement showcase with T28/T95 and IS-2 models."),
]
records = []
for name, frame, arguments, caption in plan:
    path = output / (name + ".png")
    command = ["/usr/bin/open", "-n", "-W", str(app), "--args", *arguments,
               "--release-screenshot=" + str(path),
               "--release-screenshot-frame=" + str(frame)]
    if name == "battlefield-coop":
        # Captured with this exact open command before this helper was written.
        assert path.is_file()
    else:
        assert not path.exists()
        print("Capturing " + name, flush=True)
        subprocess.run(command, check=True, timeout=75)
    records.append({"path": path.relative_to(root).as_posix(), "command": command,
                    "caption": caption, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
manifest = {
    "source_commit": attestation["source_commit"], "source_tag": attestation["source_tag"],
    "candidate_sha256": attestation["artifact_sha256"],
    "binary_sha256": prior["binary_sha256"], "captures": records,
    "scope": "Homepage previews; capture success is not physical-controller or formal release acceptance."
}
with (output / "manifest.json").open("x") as stream:
    stream.write(json.dumps(manifest, indent=2) + "\n")
