#!/usr/bin/env python3
"""Differential: 18 contiguous typed original Creative SF2 NOTE START OUTs.

Only user-owned, SHA-pinned PRIVATE original-driver captures are read.
The public executable is fed genuine post-start 148-byte SF2 descriptor
records plus observed physical voice. It reconstructs register values,
NOT the original register trace. It must then find that EXACT 18-OUT
sequence in the independent original x86 trace (same values, widths,
ports, ordering and voice) with no gaps. No emulator startup/timing or
full original-driver audio equivalence is claimed.
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path

EXPECTED_VXD_SHA = (
    "d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a"
)
EXPECTED_BANK_SHA = (
    "78f8739a25d49e519b3fdc92a7f03d4ddd68588fd0260578a880296e304b7b5f"
)


def rows_from_handoff(data: dict):
    if data.get("schema") != "125a-original-bank-to-register-handoff-v1":
        raise ValueError("Wrong original x86 trace handoff schema")
    if data.get("original_vxd_sha256") != EXPECTED_VXD_SHA:
        raise ValueError("Original SBAWE.VXD digest mismatch")
    if data.get("original_bank_sha256") != EXPECTED_BANK_SHA:
        raise ValueError("Original SYNTHGM.SF2 digest mismatch")
    rows = []
    cases = []
    for item in data.get("cases", []):
        case = item["case"]
        posts = case["after"]
        voices = case["physical_voices"]
        original_ports = case["ports"]
        if len(posts) == 0 or len(posts) != len(voices):
            raise ValueError("Original note-start descriptor/voice count mismatch")
        trace = []
        for raw in original_ports:
            if (len(raw) != 3 or
                    not 0 <= raw[0] <= 0xFFFF or raw[1] not in (1, 2, 4) or
                    not 0 <= raw[2] < (1 << (8 * raw[1]))):
                raise ValueError("Invalid original typed x86 port trace")
            trace.append((raw[0], raw[1] * 8, raw[2]))
        for layer, (desc, physical) in enumerate(zip(posts, voices)):
            try:
                raw = bytes.fromhex(desc)
            except ValueError as exc:
                raise ValueError("Non-hex original descriptor") from exc
            if len(raw) != 148 or raw[0x8C:0x8E] != b"\x01\x00":
                raise ValueError("Unsupported original SF2 note-start descriptor")
            if not 0 <= case["key"] <= 127 or not 0 <= physical < 30:
                raise ValueError("Original note key / physical voice invalid")
            rows.append(f"{raw.hex()} {case['key']} {physical}")
            cases.append((case["program"], layer, physical, trace))
    if not rows:
        raise ValueError("Empty original SF2 note-start handoff")
    return rows, cases


def check_original_ports(output: str, cases: list) -> int:
    lines = output.splitlines()
    if len(lines) != len(cases):
        raise ValueError("Native SF2 register prefix row count mismatch")
    matched = 0
    for line, (program, layer, voice, trace) in zip(lines, cases):
        tokens = line.split()
        if len(tokens) != 18:
            raise ValueError(f"GM{program} layer{layer}: expected exactly 18 OUTs")
        got = []
        for token in tokens:
            parts = token.split(":")
            if (len(parts) != 3 or len(parts[0]) != 4 or
                    len(parts[1]) != 2 or len(parts[2]) != 8):
                raise ValueError("Malformed native typed port token")
            try:
                port, bits, value = (int(x, 16) for x in parts)
            except ValueError as exc:
                raise ValueError("Nonhex native port token") from exc
            if port > 0xFFFF or bits not in (8, 16, 32) or value >= (1 << bits):
                raise ValueError("Invalid native typed port width/value")
            got.append((port, bits, value))
        # Each source original voice can be preceded by differing counts of
        # allocation/reset writes. The first GM100 layer starts at trace
        # position 10 while later layer starts at 66, so DO NOT fake a
        # constant offset/61-stride. Require one exact contiguous sequence.
        positions = [i for i in range(len(trace) - len(got) + 1)
                     if trace[i:i + len(got)] == got]
        if len(positions) != 1:
            raise ValueError(
                f"GM{program} layer{layer} voice{voice}: "
                f"original x86 exact contiguous 18 OUT matches={len(positions)}"
            )
        matched += 1
    return matched


def run() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", required=True, type=Path)
    parser.add_argument("--executable", required=True, type=Path)
    args = parser.parse_args()
    data = json.loads(args.handoff.read_text(encoding="utf-8"))
    rows, cases = rows_from_handoff(data)
    result = subprocess.run(
        [str(args.executable)],
        input="\n".join(rows) + "\n", capture_output=True,
        text=True, check=False,
    )
    if result.returncode != 0:
        raise ValueError("Native SF2 original prefix oracle failed/rejected")
    matched = check_original_ports(result.stdout, cases)
    print(
        f"P002_PRIVATE_ORIGINAL_SF2_18_TYPED_OUT_MATCH_PASS voices={matched} "
        f"port_events={matched * 18} "
        "source=independently_executed_original_x86 "
        "full_noteon_timing_pcm=NOT_PROVEN"
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(run())
    except (OSError, ValueError, KeyError, TypeError) as err:
        print(f"P002_PRIVATE_ORIGINAL_SF2_18_TYPED_OUT_MATCH_FAIL {err}",
              file=sys.stderr)
        sys.exit(1)
