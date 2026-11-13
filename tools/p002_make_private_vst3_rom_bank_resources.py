#!/usr/bin/env python3
"""Write a PRIVATE P002 ROM-SF2 VST3 source-resource pack.

The original 77,370-byte Creative SBAWE.VXD is SHA256 verified and its
actual LE page-mapped LCOD/PDAT constants are extracted by the existing
source oracle. A separate caller-supplied ORIGINAL full idle VXD state
snapshot is REQUIRED; no default or synthesized Creative state is
invented. This pack is proprietary and must never be committed/uploaded.

Usage example (paths are local/private; no original files to GitHub):
  python tools/p002_make_private_vst3_rom_bank_resources.py \
    --private-vxd C:/private/SBAWE.VXD \
    --private-idle-vxd-state C:/private/P002-idle-device-state.bin \
    --private-output C:/private/P002-original-ROM-SF2-pack.bin \
    --program 0 --channel 0 --bank 0

In the Windows VST3 host environment, supply:
 AWE64_P001_ROM_PATH=<exact hash-pinned awe32.raw>
 AWE64_P002_PRIVATE_VXD_RESOURCES_PATH=<private resource pack>
 AWE64_P002_PRIVATE_ROM_BANK_PATH=<original ROM-only format-2 SF2 bank>

EXPERIMENTAL RESEARCH MODE, not a general SF2 player. RAM sample upload,
real Win95 driver PCM parity and production real-time worst-case performance
are not proven. This generator ensures VXD source provenance, but it cannot
independently prove that a supplied state snapshot was captured from Win95.
"""
import argparse
import hashlib
import os
from pathlib import Path
import struct

# Support both python tools/script.py (local private use) and
# unittest importing tools.script (Windows GitHub Actions discovery).
if __package__:
    from .p002_extract_original_vxd_sf2_resources import (
        EXPECTED_VXD_SHA256, extract,
    )
else:
    from p002_extract_original_vxd_sf2_resources import (
        EXPECTED_VXD_SHA256, extract,
    )
ROM_WORDS = 524288
STATE_BYTES = 0x1592
MAGIC = b"125ABK01"

def build(private_vxd: bytes, state: bytes, *, program: int,
          channel: int, bank: int) -> bytes:
    resources = extract(private_vxd)  # hash-locked original LE VXD
    if (not 0 <= program <= 127 or not 0 <= channel <= 15 or
            not 0 <= bank <= 128):
        raise ValueError("Unsupported original ROM bank, program or channel")
    if len(state) != STATE_BYTES:
        raise ValueError("Private original driver state must be exactly 0x1592 bytes")
    if (struct.unpack_from("<I", state, 0x158E)[0] != 0x4D474D31 or
            struct.unpack_from("<H", state, 0x12)[0] != 0 or
            struct.unpack_from("<H", state, 0x06)[0] != 30 or
            struct.unpack_from("<I", state, 0x18)[0] < 3):
        raise ValueError("Source snapshot lacks original dry 1MGM / 30-voice state")
    # A VXD voice previously owned by another note cannot be treated as
    # independently authenticated merely because this new pack loads.
    for slot in range(30):
        marker = struct.unpack_from("<H", state, 0x42 + slot * 0x20)[0]
        if marker != 0x0FFE:
            raise ValueError(f"Source state voice {slot} not idle/verified")
    instr = resources["template_first_59_signed_words"]
    pitch_a = resources["pitch_table_a_unsigned_32"]
    pitch_b = resources["pitch_table_b_signed_8"]
    negative = resources["volume_attack128_negative_table_unsigned_16"]
    positive = resources["volume_attack128_table_unsigned_16"]
    if (len(instr) != 59 or len(pitch_a) != 128 or len(pitch_b) != 128
            or len(negative) != 15 or len(positive) != 16):
        raise ValueError("Wrong original VXD source resource geometry")
    result = bytearray(MAGIC)
    result += bytes.fromhex(EXPECTED_VXD_SHA256)
    result += struct.pack("<BBHI", program, channel, bank, ROM_WORDS)
    result += state
    # The original preset-converter initializes its 59-word base to zero;
    # the instrument uses the SHA-checked LCOD+0x2d88 template instead.
    result += struct.pack("<59h", *([0] * 59))
    result += struct.pack("<59h", *instr)
    result += struct.pack("<128I", *pitch_a)
    result += struct.pack("<128b", *pitch_b)
    result += struct.pack("<15H", *negative)
    result += struct.pack("<16H", *positive)
    assert len(result) == (8 + 32 + 8 + STATE_BYTES + 2*59*2
                           + 128*4 + 128 + 15*2 + 16*2)
    return bytes(result)

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--private-vxd", required=True, type=Path)
    parser.add_argument("--private-idle-vxd-state", required=True, type=Path)
    parser.add_argument("--private-output", required=True, type=Path)
    parser.add_argument("--program", type=int, required=True)
    parser.add_argument("--channel", type=int, required=True)
    parser.add_argument("--bank", type=int, required=True)
    a = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    destination = a.private_output.resolve()
    if destination == root or root in destination.parents:
        parser.error("Original Creative source bytes must stay OUTSIDE Git")
    if destination.exists():
        parser.error("Refusing to overwrite private original source pack")
    if destination == a.private_vxd.resolve() or destination == a.private_idle_vxd_state.resolve():
        parser.error("Cannot overwrite original source evidence")
    pack = build(a.private_vxd.read_bytes(),
                 a.private_idle_vxd_state.read_bytes(),
                 program=a.program, channel=a.channel, bank=a.bank)
    destination.parent.mkdir(parents=True, exist_ok=True)
    fd = os.open(destination, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "wb") as stream:
        stream.write(pack)
    print("P002_ORIGINAL_ROM_SF2_VST3_PRIVATE_RESOURCES_READY")
    print("private_pack_bytes", len(pack))
    print("private_pack_sha256", hashlib.sha256(pack).hexdigest())
    print("original_vxd_sha256", EXPECTED_VXD_SHA256)
    print("original_win95_pcm_parity", "NOT_PROVEN")

if __name__ == "__main__":
    main()
