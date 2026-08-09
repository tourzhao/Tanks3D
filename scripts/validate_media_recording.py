"""Fail-closed structural validation for ISO-BMFF release recordings.

``expected_identity`` uses ``(st_dev, st_ino, st_size, st_mtime_ns,
st_ctime_ns)``.  A successful call returns the normalized file suffix.  This
module validates a container structure; it does not establish what the video
shows or whether the recording is continuous.
"""

import os
from pathlib import Path
import stat


SUPPORTED_SUFFIXES = frozenset((".mov", ".mp4", ".m4v"))
MAXIMUM_BOX_COUNT = 100_000
__all__ = ("RecordingValidationError", "validate_recording")
_STRUCTURAL_TYPES = frozenset(
    (b"ftyp", b"mdat", b"moov", b"mvhd", b"trak", b"mdia", b"hdlr", b"minf", b"stbl", b"stsz")
)


class RecordingValidationError(Exception):
    """The supplied recording is not acceptable structural evidence."""


class _Box:
    __slots__ = ("kind", "start", "end", "payload_start")

    def __init__(self, kind, start, end, payload_start):
        self.kind = kind
        self.start = start
        self.end = end
        self.payload_start = payload_start

    @property
    def payload_size(self):
        return self.end - self.payload_start


def _identity(file_stat):
    return (
        file_stat.st_dev,
        file_stat.st_ino,
        file_stat.st_size,
        file_stat.st_mtime_ns,
        file_stat.st_ctime_ns,
    )


def _is_fourcc(value):
    return len(value) == 4 and all(0x20 <= byte <= 0x7E for byte in value)


class _Parser:
    def __init__(self, descriptor, file_size):
        self.descriptor = descriptor
        self.file_size = file_size
        self.box_count = 0

    def read(self, offset, length, label):
        data = bytearray()
        while len(data) < length:
            try:
                part = os.pread(
                    self.descriptor, length - len(data), offset + len(data)
                )
            except OSError as exc:
                raise RecordingValidationError(
                    "cannot read {}: {}".format(label, exc)
                ) from exc
            if not part:
                raise RecordingValidationError("truncated {}".format(label))
            data.extend(part)
        return bytes(data)

    def boxes(self, start, end, label, allow_size_zero=False):
        result = []
        offset = start
        while offset < end:
            if end - offset < 8:
                raise RecordingValidationError(
                    "{} has trailing bytes outside a box".format(label)
                )
            header = self.read(offset, 8, "{} box header".format(label))
            size32 = int.from_bytes(header[:4], "big")
            kind = header[4:8]
            if not _is_fourcc(kind):
                raise RecordingValidationError(
                    "{} contains an invalid box type".format(label)
                )
            header_size = 8
            if size32 == 1:
                large_size = self.read(offset + 8, 8, "64-bit box size")
                box_size = int.from_bytes(large_size, "big")
                header_size = 16
            elif size32 == 0:
                if not allow_size_zero:
                    raise RecordingValidationError(
                        "zero-sized box is not valid inside {}".format(label)
                    )
                box_size = end - offset
            else:
                box_size = size32
            if kind == b"uuid":
                header_size += 16
            if box_size < header_size:
                raise RecordingValidationError(
                    "{} contains a box smaller than its header".format(label)
                )
            box_end = offset + box_size
            if box_end > end:
                raise RecordingValidationError(
                    "{} contains a box beyond its boundary".format(label)
                )
            self.box_count += 1
            if self.box_count > MAXIMUM_BOX_COUNT:
                raise RecordingValidationError("recording contains too many boxes")
            result.append(_Box(kind, offset, box_end, offset + header_size))
            offset = box_end
        return result

    @staticmethod
    def exactly_one(boxes, kind, label):
        matches = [box for box in boxes if box.kind == kind]
        if len(matches) != 1:
            raise RecordingValidationError(
                "{} must contain exactly one {} box".format(
                    label, kind.decode("ascii")
                )
            )
        return matches[0]

    @staticmethod
    def reject_misnested(boxes, allowed, label):
        for box in boxes:
            if box.kind in _STRUCTURAL_TYPES and box.kind not in allowed:
                raise RecordingValidationError(
                    "{} box is misplaced inside {}".format(
                        box.kind.decode("ascii"), label
                    )
                )

    def validate_ftyp(self, box):
        if box.payload_size < 8 or box.payload_size % 4 != 0:
            raise RecordingValidationError("ftyp box has an invalid payload size")
        major_brand = self.read(box.payload_start, 4, "ftyp major brand")
        if not _is_fourcc(major_brand):
            raise RecordingValidationError("ftyp contains an invalid major brand")
        offset = box.payload_start + 8
        remaining = box.payload_size - 8
        while remaining:
            length = min(remaining, 64 * 1024)
            length -= length % 4
            brands = self.read(offset, length, "ftyp brands")
            for index in range(0, len(brands), 4):
                if not _is_fourcc(brands[index : index + 4]):
                    raise RecordingValidationError("ftyp contains an invalid brand")
            offset += length
            remaining -= length

    def validate_mvhd(self, box):
        version = self.read(box.payload_start, 1, "mvhd version")[0]
        minimum = 100 if version == 0 else 112 if version == 1 else None
        if minimum is None:
            raise RecordingValidationError("mvhd uses an unsupported version")
        if box.payload_size < minimum:
            raise RecordingValidationError("mvhd box is truncated")
        if version == 0:
            values = self.read(box.payload_start + 12, 8, "mvhd timing")
            timescale = int.from_bytes(values[:4], "big")
            duration = int.from_bytes(values[4:], "big")
            unknown_duration = 0xFFFFFFFF
        else:
            values = self.read(box.payload_start + 20, 12, "mvhd timing")
            timescale = int.from_bytes(values[:4], "big")
            duration = int.from_bytes(values[4:], "big")
            unknown_duration = 0xFFFFFFFFFFFFFFFF
        if timescale == 0 or duration == 0 or duration == unknown_duration:
            raise RecordingValidationError(
                "mvhd must have a positive finite timescale and duration"
            )

    def handler_type(self, box):
        if box.payload_size < 24:
            raise RecordingValidationError("hdlr box is truncated")
        header = self.read(box.payload_start, 12, "hdlr header")
        if header[0] != 0:
            raise RecordingValidationError("hdlr uses an unsupported version")
        return header[8:12]

    def sample_count(self, box):
        if box.payload_size < 12:
            raise RecordingValidationError("stsz box is truncated")
        header = self.read(box.payload_start, 12, "stsz header")
        if header[0] != 0:
            raise RecordingValidationError("stsz uses an unsupported version")
        sample_size = int.from_bytes(header[4:8], "big")
        sample_count = int.from_bytes(header[8:12], "big")
        expected_size = 12 if sample_size else 12 + 4 * sample_count
        if box.payload_size != expected_size:
            raise RecordingValidationError("stsz sample table size is inconsistent")
        return sample_count

    def validate_track(self, box):
        children = self.boxes(box.payload_start, box.end, "trak")
        self.reject_misnested(children, frozenset((b"mdia",)), "trak")
        mdia = self.exactly_one(children, b"mdia", "trak")
        media_children = self.boxes(mdia.payload_start, mdia.end, "mdia")
        self.reject_misnested(
            media_children, frozenset((b"hdlr", b"minf")), "mdia"
        )
        handler = self.exactly_one(media_children, b"hdlr", "mdia")
        minf = self.exactly_one(media_children, b"minf", "mdia")
        minf_children = self.boxes(minf.payload_start, minf.end, "minf")
        self.reject_misnested(minf_children, frozenset((b"stbl",)), "minf")
        stbl = self.exactly_one(minf_children, b"stbl", "minf")
        table_children = self.boxes(stbl.payload_start, stbl.end, "stbl")
        self.reject_misnested(table_children, frozenset((b"stsz",)), "stbl")
        stsz = self.exactly_one(table_children, b"stsz", "stbl")
        return self.handler_type(handler), self.sample_count(stsz)

    def validate_moov(self, box):
        children = self.boxes(box.payload_start, box.end, "moov")
        self.reject_misnested(children, frozenset((b"mvhd", b"trak")), "moov")
        mvhd = self.exactly_one(children, b"mvhd", "moov")
        self.validate_mvhd(mvhd)
        tracks = [child for child in children if child.kind == b"trak"]
        if not tracks:
            raise RecordingValidationError("moov contains no tracks")
        has_video_samples = False
        for track in tracks:
            handler, samples = self.validate_track(track)
            if handler == b"vide" and samples > 0:
                has_video_samples = True
        if not has_video_samples:
            raise RecordingValidationError(
                "moov has no video track with a positive stsz sample count"
            )

    def validate(self):
        boxes = self.boxes(0, self.file_size, "top level", allow_size_zero=True)
        if not boxes or boxes[0].kind != b"ftyp":
            raise RecordingValidationError("ftyp must be the first top-level box")
        self.reject_misnested(
            boxes, frozenset((b"ftyp", b"mdat", b"moov")), "top level"
        )
        ftyp = self.exactly_one(boxes, b"ftyp", "top level")
        moov = self.exactly_one(boxes, b"moov", "top level")
        mdats = [box for box in boxes if box.kind == b"mdat"]
        if not mdats or not any(box.payload_size > 0 for box in mdats):
            raise RecordingValidationError("recording has no nonempty mdat box")
        self.validate_ftyp(ftyp)
        self.validate_moov(moov)


def validate_recording(
    path, minimum_bytes, maximum_bytes, expected_identity=None
):
    """Validate an ISO-BMFF recording and return its normalized suffix."""

    if (
        isinstance(minimum_bytes, bool)
        or not isinstance(minimum_bytes, int)
        or isinstance(maximum_bytes, bool)
        or not isinstance(maximum_bytes, int)
        or minimum_bytes < 0
        or maximum_bytes < minimum_bytes
    ):
        raise RecordingValidationError("invalid recording size bounds")
    if expected_identity is not None and (
        not isinstance(expected_identity, tuple)
        or len(expected_identity) != 5
        or any(
            isinstance(value, bool) or not isinstance(value, int)
            for value in expected_identity
        )
    ):
        raise RecordingValidationError(
            "expected identity must be a five-integer tuple"
        )
    recording_path = Path(path)
    suffix = recording_path.suffix.lower()
    if suffix not in SUPPORTED_SUFFIXES:
        raise RecordingValidationError("recording must use mov, mp4, or m4v")
    if not hasattr(os, "O_NOFOLLOW"):
        raise RecordingValidationError("this platform cannot open recordings nofollow")
    flags = os.O_RDONLY | os.O_NOFOLLOW | getattr(os, "O_NONBLOCK", 0)
    descriptor = -1
    try:
        try:
            descriptor = os.open(str(recording_path), flags)
        except OSError as exc:
            raise RecordingValidationError(
                "cannot open recording without following links: {}".format(exc)
            ) from exc
        before = os.fstat(descriptor)
        before_identity = _identity(before)
        if not stat.S_ISREG(before.st_mode):
            raise RecordingValidationError("recording must be a regular file")
        if expected_identity is not None and before_identity != expected_identity:
            raise RecordingValidationError("recording identity does not match")
        if before.st_size < minimum_bytes or before.st_size > maximum_bytes:
            raise RecordingValidationError("recording size is outside the allowed range")
        _Parser(descriptor, before.st_size).validate()
        after = os.fstat(descriptor)
        if _identity(after) != before_identity:
            raise RecordingValidationError("recording changed while being validated")
        return suffix
    except OSError as exc:
        raise RecordingValidationError("cannot inspect recording: {}".format(exc)) from exc
    finally:
        if descriptor >= 0:
            os.close(descriptor)
