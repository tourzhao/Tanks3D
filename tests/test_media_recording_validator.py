#!/usr/bin/env python3
"""Focused tests for the dependency-free ISO-BMFF recording validator."""

import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import unittest


sys.dont_write_bytecode = True
REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
VALIDATOR_PATH = REPOSITORY_ROOT / "scripts/validate_media_recording.py"
SPEC = importlib.util.spec_from_file_location("validate_media_recording", VALIDATOR_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("cannot load media recording validator")
VALIDATOR = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = VALIDATOR
SPEC.loader.exec_module(VALIDATOR)

from media_recording_fixture import (  # noqa: E402
    box,
    recording,
)


MINIMUM_BYTES = 64 * 1024
MAXIMUM_BYTES = 2 * 1024 * 1024


def file_identity(path):
    value = os.stat(path, follow_symlinks=False)
    return (
        value.st_dev,
        value.st_ino,
        value.st_size,
        value.st_mtime_ns,
        value.st_ctime_ns,
    )


class MediaRecordingValidatorTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name).resolve()
        self.path = self.root / "launch.mov"

    def tearDown(self):
        self.temporary.cleanup()

    def write(self, data=None, path=None):
        target = path or self.path
        target.write_bytes(recording() if data is None else data)
        return target

    def assert_rejected(self, path=None):
        with self.assertRaises(VALIDATOR.RecordingValidationError):
            VALIDATOR.validate_recording(
                path or self.path, MINIMUM_BYTES, MAXIMUM_BYTES
            )

    def test_valid_32_and_64_bit_box_layouts(self):
        self.write()
        identity = file_identity(self.path)
        self.assertEqual(
            VALIDATOR.validate_recording(
                self.path, MINIMUM_BYTES, MAXIMUM_BYTES, identity
            ),
            ".mov",
        )
        self.write(recording(large_mdat=True))
        self.assertEqual(
            VALIDATOR.validate_recording(self.path, MINIMUM_BYTES, MAXIMUM_BYTES),
            ".mov",
        )

    def test_ftyp_followed_by_zero_padding_is_not_a_recording(self):
        ftyp = box(b"ftyp", b"qt  " + b"\0\0\0\0")
        self.write(ftyp.ljust(MINIMUM_BYTES, b"\0"))
        self.assert_rejected()

    def test_malformed_box_size_is_rejected(self):
        data = bytearray(recording())
        ftyp_size = int.from_bytes(data[:4], "big")
        data[ftyp_size : ftyp_size + 4] = (len(data) + 1).to_bytes(4, "big")
        self.write(data)
        self.assert_rejected()

    def test_zero_duration_is_rejected(self):
        self.write(recording(duration=0))
        self.assert_rejected()

    def test_no_video_track_is_rejected(self):
        self.write(recording(handler=b"soun"))
        self.assert_rejected()

    def test_zero_video_samples_is_rejected(self):
        self.write(recording(sample_count=0))
        self.assert_rejected()

    def test_empty_mdat_is_rejected(self):
        self.write(recording(empty_mdat=True))
        self.assert_rejected()

    def test_wrong_extension_is_rejected(self):
        path = self.write(path=self.root / "launch.webm")
        self.assert_rejected(path)

    def test_identity_must_match_the_open_file(self):
        self.write()
        identity = list(file_identity(self.path))
        identity[1] += 1
        with self.assertRaises(VALIDATOR.RecordingValidationError):
            VALIDATOR.validate_recording(
                self.path, MINIMUM_BYTES, MAXIMUM_BYTES, tuple(identity)
            )

    def test_duplicate_and_misnested_critical_boxes_are_rejected(self):
        self.write(recording(duplicate_mvhd=True))
        self.assert_rejected()
        self.write(recording(misplaced=True))
        self.assert_rejected()

    def test_symlink_is_not_followed(self):
        target = self.write()
        link = self.root / "linked.mov"
        link.symlink_to(target)
        self.assert_rejected(link)


if __name__ == "__main__":
    unittest.main()
