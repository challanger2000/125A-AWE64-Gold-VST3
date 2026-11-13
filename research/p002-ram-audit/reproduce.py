#!/usr/bin/env python3
"""Synthetic counterexamples against PR4 9cc1322; no vendor assets.

Exit zero means the documented observations were reproduced, NOT that
the capture implementation passed a correctness or Creative parity test.
Run from any directory: python research/p002-ram-audit/reproduce.py
"""
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.p002_original_ram_trace_probe import (
    scan_capture, filter_original_ram_upload_port_writes,
)

BASE = 0x200400


def upload(words, frame=0):
    return [f"{frame} E22 0036", f"{frame} A20 0400",
            f"{frame} A22 0020", f"{frame} E22 003A"] + [
                f"{frame} A20 {w:04X}" for w in words]


def filtered_rejection(trace):
    try:
        list(filter_original_ram_upload_port_writes(trace))
    except ValueError as exc:
        return str(exc)
    return None


def main():
    words = list(range(100, 120))
    pcm = struct.pack("<20H", *words)
    baseline = scan_capture(upload(words), pcm)
    assert baseline["direct_pcm16_upload_matches"] == 1
    assert baseline["first_match_emu8000_word_address"] == BASE

    overwritten = upload(words) + upload([0xDEAD], 1)
    stale = scan_capture(overwritten, pcm)
    assert stale["direct_pcm16_upload_matches"] == 1
    # Literal hardware state after the final write differs at first word.
    assert 0xDEAD != words[0]

    partial = upload(words) + ["1 E22 0036", "1 A20 1234"]
    dangling = scan_capture(partial, pcm)
    assert dangling["direct_pcm16_upload_matches"] == 1
    rejected = filtered_rejection(partial)
    assert rejected is not None

    # Byte hits at 1,2,3. Only byte offset 2 is WORD-aligned.
    # find(..., previous+2) stays on odd offsets 1,3 and misses 2.
    expected = b"\x11" * 32
    raw = b"\0" + b"\x11" * 34 + b"\0"
    correct_offsets = [i for i in range(0, len(raw)-len(expected)+1, 2)
                       if raw[i:i+len(expected)] == expected]
    assert correct_offsets == [2]
    skipped = scan_capture(upload(struct.unpack("<18H", raw)), expected)
    assert skipped["direct_pcm16_upload_matches"] == 0
    print(json.dumps({
        "base_commit": "9cc1322d8d9813e0bed05dec2f2d9c6651209236",
        "assets": "synthetic only",
        "baseline": baseline,
        "later_overwrite_still_matches": stale,
        "partial_pointer_still_matches": dangling,
        "partial_pointer_export_rejection": rejected,
        "aligned_match_missed": skipped,
        "independent_aligned_match_byte_offsets": correct_offsets,
        "meaning": "counterexamples reproduced; NOT driver/PCM parity"
    }, indent=2))


if __name__ == "__main__":
    main()
