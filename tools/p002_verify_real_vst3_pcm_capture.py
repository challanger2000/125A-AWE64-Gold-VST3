#!/usr/bin/env python3
"""P002 verify TWO independent real-exported-VST3 P001 ROM MIDI WAV captures.

This tests repeatability of the SOURCE pre-card diagnostic rendering in
the actual VST3 DLL, NOT Creative Win95 guest output, RAM SF2, CT1745,
or an independent comparison to original hardware. No private ROM or
WAV content is copied into GitHub artifacts. WAVs remain ephemeral.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct
import sys
import wave

EXPECTED_FRAMES = 512 * 36
NOTEON_FRAME = 137
EXPECTED_BYTES = 44 + 4 * EXPECTED_FRAMES
MAX_BYTES = 4 * 1024 * 1024


def inspect(path: Path) -> dict:
    if not path.is_file() or not 44 <= path.stat().st_size <= MAX_BYTES:
        raise ValueError("actual VST3 capture WAV is missing or oversized")
    data = path.read_bytes()
    if len(data) != EXPECTED_BYTES:
        raise ValueError("actual VST3 capture has unexpected file length")
    if (data[:4] != b"RIFF" or data[8:12] != b"WAVE"
            or data[12:16] != b"fmt " or data[36:40] != b"data"
            or struct.unpack_from("<I", data, 4)[0] != len(data) - 8
            or struct.unpack_from("<I", data, 16)[0] != 16
            or struct.unpack_from("<I", data, 40)[0] != EXPECTED_FRAMES * 4):
        raise ValueError("actual VST3 capture has invalid exact WAV RIFF sizes")
    with wave.open(str(path), "rb") as inp:
        if (inp.getnchannels() != 2 or inp.getframerate() != 44100
                or inp.getsampwidth() != 2 or inp.getcomptype() != "NONE"
                or inp.getnframes() != EXPECTED_FRAMES):
            raise ValueError("actual VST3 pre-card tap is not stereo 44100/PCM16")
        pcm = inp.readframes(EXPECTED_FRAMES)
        if len(pcm) != EXPECTED_FRAMES * 4:
            raise ValueError("actual VST3 audio truncated")
    if any(pcm[:NOTEON_FRAME * 4]):
        raise ValueError("actual VST3 produced PCM before MIDI NoteOn at frame 137")
    nonzero = 0
    peak = 0
    for (word,) in struct.iter_unpack("<h", pcm[NOTEON_FRAME * 4:]):
        if word:
            nonzero += 1
        peak = max(peak, abs(word))
    if nonzero <= 1000 or peak < 64:
        raise ValueError("real exported VST3 ROM note WAV is silent or implausibly quiet")
    return {"sha256": hashlib.sha256(data).hexdigest(),
            "stereo_frames": EXPECTED_FRAMES,
            "noteon_frame": NOTEON_FRAME,
            "nonzero_channel_samples_after_noteon": nonzero,
            "peak_pcm16": peak}


def validate_pair(first: Path, second: Path) -> dict:
    if first.resolve() == second.resolve():
        raise ValueError("two independently rendered VST3 files are required")
    a = inspect(first)
    b = inspect(second)
    if a["sha256"] != b["sha256"]:
        raise ValueError("independent exported VST3 NoteOn PCM16 captures differ")
    return a


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture-a", type=Path, required=True)
    parser.add_argument("--capture-b", type=Path, required=True)
    args = parser.parse_args()
    evidence = validate_pair(args.capture_a, args.capture_b)
    print(
        "P002_REAL_EXPORTED_WINDOWS_VST3_PRECARD_PCM16_WAV_DETERMINISM_PASS",
        "frames=", evidence["stereo_frames"],
        "noteon_frame=", evidence["noteon_frame"],
        "peak_pcm16=", evidence["peak_pcm16"],
        "sha256=", evidence["sha256"],
        "original_win95_creative_ram_SF2_pcm_parity=NOT_TESTED",
        "commercial_release=NOT_APPROVED",
    )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, wave.Error) as exc:
        print("P002_REAL_VST3_PRECARD_WAV_NO_GO", str(exc), file=sys.stderr)
        sys.exit(1)
