#!/usr/bin/env python3
"""Verify the user's original EMU8000 ROM and Creative 1MGM SBK privately.

Identity/RIFF checks only. Do not upload the media to GitHub or assume that
successful verification proves rendered notes or fork PCM parity.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import struct

ROM_SHA = "4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e"
SBK_SHA = "fd2e35a260fef22fe809918dda3c921c6d48d53a29c3515af8a41019ce72d586"


def demand(ok: bool, reason: str) -> None:
    if not ok:
        raise ValueError(reason)


def chunks(data: bytes, start: int, end: int):
    pos = start
    while pos < end:
        demand(pos + 8 <= end, "Incomplete RIFF chunk header")
        key = data[pos:pos + 4]
        length = struct.unpack_from("<I", data, pos + 4)[0]
        begin = pos + 8
        finish = begin + length + (length & 1)
        demand(finish <= end, "RIFF chunk exceeds declared bounds")
        yield key, data[begin:begin + length]
        pos = finish
    demand(pos == end, "Invalid RIFF chunk alignment")


def checked_file(path: Path, expected_size: int, sha256: str) -> bytes:
    demand(path.is_file(), f"Private media absent: {path.name}")
    data = path.read_bytes()
    demand(len(data) == expected_size, f"Wrong media size: {path.name}")
    actual = hashlib.sha256(data).hexdigest()
    demand(actual == sha256, f"Wrong media SHA256: {path.name} computed {actual}")
    return data


def verify(rom: Path, sbk: Path) -> None:
    original_rom = checked_file(rom, 1048576, ROM_SHA)
    # Original pinned emu8k_init() detects this 2-byte AWE-DUMP offset
    # from the 16-bit little-endian words at indices 3 and 4.
    demand(original_rom[6:10] == b"M1MG", "Unexpected original AWE-DUMP ROM layout")

    original_sbk = checked_file(sbk, 34832, SBK_SHA)
    demand(original_sbk[:4] == b"RIFF" and original_sbk[8:12] == b"sfbk",
           "Creative bank lacks RIFF/sfbk identity")
    demand(struct.unpack_from("<I", original_sbk, 4)[0] + 8 == len(original_sbk),
           "Creative bank RIFF length mismatch")
    sections = {}
    for tag, payload in chunks(original_sbk, 12, len(original_sbk)):
        demand(tag == b"LIST" and len(payload) >= 4, "Unexpected SBK top-level chunk")
        section_name = payload[:4]
        demand(section_name not in sections, "Duplicate bank LIST section")
        sections[section_name] = dict(chunks(payload, 4, len(payload)))
    demand(set(sections) == {b"INFO", b"sdta", b"pdta"},
           "Missing or unexpected bank section")
    info = sections[b"INFO"]
    for key, value in ((b"isng", b"EMU8000"), (b"irom", b"1MGM"),
                       (b"IPRD", b"SBAWE32")):
        demand(info.get(key, b"").rstrip(b"\0") == value,
               "Creative ROM descriptor mismatch: " + key.decode("ascii"))

    pdta = sections[b"pdta"]
    demand(len(pdta.get(b"phdr", b"")) == 130 * 38,
           "Original Creative preset-header inventory mismatch")
    # This original SBK's shdr has 51 x 48-byte Creative records,
    # *not* the 46-byte standard SF2 sample-header layout.
    demand(len(pdta.get(b"shdr", b"")) == 51 * 48,
           "Creative 48-byte sample-header inventory mismatch")
    demand(b"smpl" not in sections[b"sdta"],
           "This ROM descriptor must not claim embedded PCM samples")
    print("P001 ORIGINAL_ROM_IDENTITY_PASS bytes=1048576 sha256=" + ROM_SHA
          + " offset=AWE-DUMP-2")
    print("P001 ORIGINAL_CREATIVE_SBK_IDENTITY_PASS bytes=34832 sha256=" + SBK_SHA
          + " isng=EMU8000 irom=1MGM product=SBAWE32")
    print("SCOPE=PRIVATE_MEDIA_IDENTITY_ONLY; ROM_NOTE_PCM=NOT_RUN")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--rom", required=True, type=Path)
    p.add_argument("--sbk", required=True, type=Path)
    args = p.parse_args()
    try:
        verify(args.rom, args.sbk)
    except (ValueError, OSError) as e:
        p.exit(1, "P001 ORIGINAL_MEDIA_NOT_VERIFIED: " + str(e) + "\n")


if __name__ == "__main__":
    main()
