#!/usr/bin/env python3
"""Private original Creative PCOD+0x3735 rate→dwPitch native differential.

Caller must own ORIGINAL 77,370-byte SBAWE.VXD. The exact hash, LE page map
and table sizes are validated by the shared loader. Never upload raw driver
tables, code or 192000 pitch values into GitHub logs/artifacts.
"""
import argparse
import hashlib
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.p002_extract_original_vxd_sf2_resources import extract


def original_reference(rate: int, base: list[int], slope: list[int]) -> int:
    """Independent Python transcription of original x86 PCOD+0x3735."""
    if rate < 1:
        raise ValueError("Only positive sample rates supported")
    p = (rate * 0x2F8E) & 0xFFFFFFFF
    if p == 0:
        raise ValueError("Original low-32-bit multiplication yielded zero")
    shifts = 0
    while not p & 0x80000000:
        p = (p << 1) & 0xFFFFFFFF
        shifts += 1
    index = (p >> 24) & 0x7F
    fraction = (p >> 17) & 0x7F
    exponent = ((31 - shifts - 15) << 20) & 0xFFFFFFFF
    return (exponent + base[index] + slope[index] * fraction) & 0xFFFFFFFF


def compare_native(executable: Path, resources: dict,
                   rates: int = 192000) -> tuple[int, str]:
    bases = resources["pitch_table_a_unsigned_32"]
    slopes = resources["pitch_table_b_signed_8"]
    if len(bases) != 128 or len(slopes) != 128:
        raise ValueError("Original VXD pitch tables are not 128 DWORD/128 BYTE")
    if not 1 <= rates <= 192000:
        raise ValueError("Unsupported sample-rate matrix length")
    payload = " ".join(map(str, bases + slopes)) + "\n"
    result = subprocess.run(
        [str(executable), "--pitch-matrix"],
        input=payload, text=True, capture_output=True, check=False,
    )
    if result.returncode:
        raise ValueError("Native original table oracle rejected input")
    lines = result.stdout.splitlines()
    if len(lines) != 192000:
        raise ValueError("Incomplete native pitch-matrix output")
    sha = hashlib.sha256()
    for i in range(rates):
        line = lines[i]
        if len(line) != 8 or any(c not in "0123456789abcdef" for c in line):
            raise ValueError(f"rate {i + 1}: noncanonical output")
        got = int(line, 16)
        expected = original_reference(i + 1, bases, slopes)
        if got != expected:
            # Avoid publishing original table values or pitch traces.
            raise ValueError(f"Original SF2 pitch mismatch at sample rate {i + 1}")
        sha.update(struct.pack("<I", got))
    return rates, sha.hexdigest()


def run() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--private-vxd", type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    resources = extract(args.private_vxd.read_bytes())
    n, digest = compare_native(args.executable, resources)
    print(
        f"P002_PRIVATE_ORIGINAL_SF2_PITCH_MATRIX_PASS rates={n} "
        f"native_equal_python_reference=TRUE digest_sha256={digest} "
        "actual_original_x86_192000_rerun=NOT_PROVEN pcm=NOT_PROVEN"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(run())
    except (ValueError, OSError) as exc:
        print(f"P002_PRIVATE_ORIGINAL_SF2_PITCH_MATRIX_FAIL: {exc}",
              file=sys.stderr)
        sys.exit(1)
