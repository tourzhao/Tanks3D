"""Packaged audio acceptance must exercise the PCK and fail without evidence."""

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
SPEC = importlib.util.spec_from_file_location("godot_audio_gate", ROOT / "scripts/test_godot_audio.py")
AUDIO = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIO)


class GodotAudioGateTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="tanks-audio-gate-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.app = self.root / "build/Tanks3D-Godot.app"
        self.logs = self.root / "build/godot/audio-package-validation"
        script = self.root / "godot/sample/audio_checks.gd"
        script.parent.mkdir(parents=True)
        script.write_text('extends SceneTree\nconst Bank = preload("res://audio_bank.gd")\n')
        cues = [path.stem for path in (ROOT / "resources/sounds").glob("*.ogg")]
        sounds = self.root / "resources/sounds"
        sounds.mkdir(parents=True)
        for cue in cues:
            (sounds / (cue + ".ogg")).touch()
        checks = ["import-only", "resources", "lazy-pool", "reuse", "voice-limit", "single", "priority",
                  "engine-exclusive", "disabled", "zero-gain", "decoded-mixer", "routing",
                  "overlap-limit", "zero-mute", "volume-restore", "stop"]
        self.output = "TANKS_AUDIO_CHECKS_PASSED " + json.dumps({
            "resources": 22, "checks": checks, "mixed_cues": {cue: 0.125 for cue in cues},
        })
        self.manifest = {"staged_sha256": {"audio_bank.gd": "source"},
                         "pack_entries_sha256": {"res://audio_bank.gd": "packed"}}
        for target, name, value in [(AUDIO, "ROOT", self.root), (AUDIO.gate, "ROOT", self.root),
                                    (AUDIO.package, "APP", self.app)]:
            patch = mock.patch.object(target, name, value)
            patch.start()
            self.addCleanup(patch.stop)
        self.dependencies = mock.patch.object(AUDIO.gate, "validate_dependencies", return_value={})
        self.dependencies.start()
        self.addCleanup(self.dependencies.stop)
        verifier = mock.patch.object(AUDIO.package, "verify", return_value=self.manifest)
        self.verify = verifier.start()
        self.addCleanup(verifier.stop)

    def completed(self, output=None):
        return subprocess.CompletedProcess([], 0, self.output if output is None else output)

    def test_packaged_mixer_uses_only_the_app_pack_from_external_cwd(self):
        with mock.patch.object(AUDIO.gate.subprocess, "run", return_value=self.completed()) as run, \
                mock.patch.object(AUDIO.gate, "validate_staged") as staged:
            AUDIO.main(["--packaged"])
        command = run.call_args.args[0]
        self.assertEqual(Path(command[0]), self.app / "Contents/MacOS/Tanks3D-Godot")
        self.assertNotIn("--path", command)
        self.assertNotIn("--headless", command)
        self.assertIn("--require-import-only", command)
        self.assertEqual(run.call_args.kwargs["cwd"], "/private/tmp")
        script = Path(command[command.index("--script") + 1])
        self.assertEqual(script.parent, self.logs)
        self.assertEqual(script.read_bytes(), (self.root / "godot/sample/audio_checks.gd").read_bytes())
        staged.assert_not_called()
        receipt = json.loads((self.logs / "result.json").read_text())
        self.assertEqual(receipt["resource_source"], "standalone app PCK")
        self.assertEqual(receipt["pack_entries_sha256"], self.manifest["pack_entries_sha256"])
        self.assertEqual(self.verify.call_count, 2)

    def test_resource_only_report_cannot_create_a_mixer_receipt(self):
        output = self.output.replace('"decoded-mixer", ', '')
        self.logs.mkdir(parents=True)
        (self.logs / "result.json").write_text('{"status":"passed"}')
        with mock.patch.object(AUDIO.gate.subprocess, "run", return_value=self.completed(output)), \
                self.assertRaisesRegex(RuntimeError, "decoded samples"):
            AUDIO.main(["--packaged"])
        self.assertFalse((self.logs / "result.json").exists())

    def test_decoding_without_source_absence_check_is_rejected(self):
        output = self.output.replace('"import-only", ', '')
        with mock.patch.object(AUDIO.gate.subprocess, "run", return_value=self.completed(output)), \
                self.assertRaisesRegex(RuntimeError, "raw source assets"):
            AUDIO.main(["--packaged"])
        self.assertFalse((self.logs / "result.json").exists())

    def test_engine_error_rejects_a_zero_exit_and_success_marker(self):
        with mock.patch.object(AUDIO.gate.subprocess, "run",
                               return_value=self.completed("ERROR: missing packed resource\n" + self.output)), \
                self.assertRaisesRegex(RuntimeError, "mixer failed"):
            AUDIO.main(["--packaged"])
        self.assertFalse((self.logs / "result.json").exists())

    def test_changed_pack_is_rejected_after_decoding(self):
        self.verify.side_effect = [self.manifest, dict(self.manifest, pack_entries_sha256={})]
        with mock.patch.object(AUDIO.gate.subprocess, "run", return_value=self.completed()), \
                self.assertRaisesRegex(RuntimeError, "runtime changed"):
            AUDIO.main(["--packaged"])
        self.assertFalse((self.logs / "result.json").exists())

    def test_changed_external_script_is_rejected_after_decoding(self):
        def changed(*args, **kwargs):
            (self.logs / "audio_checks.gd").write_text("changed while testing\n")
            return self.completed()
        with mock.patch.object(AUDIO.gate.subprocess, "run", side_effect=changed), \
                self.assertRaisesRegex(RuntimeError, "test script changed"):
            AUDIO.main(["--packaged"])
        self.assertFalse((self.logs / "result.json").exists())


if __name__ == "__main__":
    unittest.main()
