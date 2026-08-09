"""Build small structurally valid ISO-BMFF recordings for release tests."""


MINIMUM_RECORDING_BYTES = 64 * 1024


def box(kind, payload=b"", large=False):
    if len(kind) != 4:
        raise ValueError("ISO-BMFF box type must contain four bytes")
    if large:
        return (
            b"\0\0\0\1"
            + kind
            + (16 + len(payload)).to_bytes(8, "big")
            + payload
        )
    return (8 + len(payload)).to_bytes(4, "big") + kind + payload


def mvhd(duration=9000):
    payload = bytearray(100)
    payload[12:16] = (1000).to_bytes(4, "big")
    payload[16:20] = duration.to_bytes(4, "big")
    payload[20:24] = (0x00010000).to_bytes(4, "big")
    payload[24:26] = (0x0100).to_bytes(2, "big")
    payload[96:100] = (2).to_bytes(4, "big")
    return box(b"mvhd", bytes(payload))


def track(handler=b"vide", sample_count=1, misplaced=False):
    hdlr = box(b"hdlr", b"\0" * 8 + handler + b"\0" * 12)
    stsz = box(
        b"stsz",
        b"\0" * 4 + (4).to_bytes(4, "big") + sample_count.to_bytes(4, "big"),
    )
    mdia = box(b"mdia", hdlr + box(b"minf", box(b"stbl", stsz)))
    return box(b"trak", (hdlr if misplaced else b"") + mdia)


def recording(
    duration=9000,
    handler=b"vide",
    sample_count=1,
    large_mdat=False,
    empty_mdat=False,
    duplicate_mvhd=False,
    misplaced=False,
    minimum_bytes=MINIMUM_RECORDING_BYTES,
):
    ftyp = box(b"ftyp", b"qt  " + b"\0\0\0\0" + b"qt  ")
    movie_payload = mvhd(duration)
    if duplicate_mvhd:
        movie_payload += mvhd(duration)
    movie_payload += track(handler, sample_count, misplaced)
    media = box(b"mdat", b"" if empty_mdat else b"frame-data", large=large_mdat)
    data = ftyp + media + box(b"moov", movie_payload)
    padding = max(8, minimum_bytes - len(data))
    return data + box(b"free", b"\0" * (padding - 8))
