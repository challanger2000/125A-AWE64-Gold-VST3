#!/usr/bin/env python3
"""Strict P002 comparison of TWO externally captured dry PCM16 WAV windows.

Use only AFTER separate evidence proves identical ROM, original Creative
Win95 driver bank, MIDI, note timings, EMU8000 register sequence, zero FX
sends, 44.1-kHz clock and identical PRE-CARD AWE32_WAV/PCM16 signal taps.

The tool never auto-aligns, changes sample rate, normalizes gain, truncates
data silently, guesses soundbank origin or certifies Creative provenance.
Original boot/DAW silence is excluded ONLY by the user's explicit frame
offsets. Private WAVs and reports must stay OUTSIDE the public repository.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import wave

RATE = 44100
MAX_WAV_BYTES = 1_000_000_000
MAX_COMPARE_FRAMES = RATE * 300
CHUNK_FRAMES = 4096
MIN_NONZERO_SAMPLES = 64


def _require(condition: bool, reason: str) -> None:
    if not condition:
        raise ValueError(reason)


def _validate_wav(audio: wave.Wave_read, label: str) -> None:
    _require(
        audio.getnchannels() == 2
        and audio.getsampwidth() == 2
        and audio.getframerate() == RATE
        and audio.getcomptype() == "NONE",
        f"{label}: expected 44.1kHz stereo 16-bit linear PCM WAV "
        "(pre-card AWE32_WAV tap ONLY)",
    )


def compare_pcm16_windows(
    original: Path,
    native: Path,
    original_start_frame: int,
    native_start_frame: int,
    frames: int,
) -> dict:
    original = original.resolve()
    native = native.resolve()
    _require(original != native, "Original and VST3 input WAV must be distinct files")
    _require(
        256 <= frames <= MAX_COMPARE_FRAMES
        and original_start_frame >= 0
        and native_start_frame >= 0,
        "Explicit nonnegative start frames and 256..13,230,000 frames required",
    )
    for label, path in (("Original", original), ("VST3", native)):
        _require(path.is_file(), f"{label}: WAV missing")
        _require(44 <= path.stat().st_size <= MAX_WAV_BYTES,
                 f"{label}: WAV missing or over safety size limit")

    with wave.open(str(original), "rb") as source, \
         wave.open(str(native), "rb") as candidate:
        _validate_wav(source, "Original")
        _validate_wav(candidate, "VST3")
        _require(original_start_frame + frames <= source.getnframes(),
                 "Original window exceeds available source WAV frames")
        _require(native_start_frame + frames <= candidate.getnframes(),
                 "VST3 window exceeds available native WAV frames")
        source.setpos(original_start_frame)
        candidate.setpos(native_start_frame)

        sha_source = hashlib.sha256()
        sha_native = hashlib.sha256()
        source_squares = native_squares = error_squares = 0
        source_peak = native_peak = max_error = 0
        source_nonzero = native_nonzero = 0
        mismatched_samples = mismatched_frames = 0
        first_mismatched_frame = None
        compared = 0

        while compared < frames:
            count = min(CHUNK_FRAMES, frames - compared)
            original_bytes = source.readframes(count)
            native_bytes = candidate.readframes(count)
            _require(len(original_bytes) == len(native_bytes) == count * 4,
                     "Unexpected truncated stereo PCM window")
            sha_source.update(original_bytes)
            sha_native.update(native_bytes)
            originals = struct.iter_unpack("<hh", original_bytes)
            natives = struct.iter_unpack("<hh", native_bytes)
            for i, ((sl, sr), (nl, nr)) in enumerate(zip(originals, natives)):
                unequal = False
                for a, b in ((sl, nl), (sr, nr)):
                    diff = a - b
                    source_squares += a * a
                    native_squares += b * b
                    error_squares += diff * diff
                    source_peak = max(source_peak, abs(a))
                    native_peak = max(native_peak, abs(b))
                    max_error = max(max_error, abs(diff))
                    source_nonzero += a != 0
                    native_nonzero += b != 0
                    if diff:
                        mismatched_samples += 1
                        unequal = True
                if unequal:
                    mismatched_frames += 1
                    if first_mismatched_frame is None:
                        first_mismatched_frame = compared + i
            compared += count

    _require(source_nonzero >= MIN_NONZERO_SAMPLES and source_peak >= 64,
             "Original selection is silence or lacks musical-range PCM; "
             "explicitly select actual note playback, not VM boot silence")
    _require(native_nonzero >= MIN_NONZERO_SAMPLES and native_peak >= 64,
             "VST3 selection is silence or lacks musical-range PCM")
    n = frames * 2
    return {
        "status": "EXACT_PRECARD_PCM16_EQUAL" if not mismatched_samples
                  else "PRECARD_PCM16_MISMATCH",
        "sample_exact": mismatched_samples == 0,
        "source_domain": "PRE_CARD_AWE32_WAV_PCM16_ONLY",
        "original_start_frame": original_start_frame,
        "vst3_start_frame": native_start_frame,
        "compared_stereo_frames": frames,
        "compared_channel_samples": n,
        "original_window_sha256": sha_source.hexdigest(),
        "vst3_window_sha256": sha_native.hexdigest(),
        "mismatched_channel_samples": mismatched_samples,
        "mismatched_stereo_frames": mismatched_frames,
        "first_mismatched_relative_frame": first_mismatched_frame,
        "max_abs_difference_pcm16_lsb": max_error,
        "rms_original_pcm16_lsb": math.sqrt(source_squares / n),
        "rms_vst3_pcm16_lsb": math.sqrt(native_squares / n),
        "rms_error_pcm16_lsb": math.sqrt(error_squares / n),
        "peak_original_pcm16_lsb": source_peak,
        "peak_vst3_pcm16_lsb": native_peak,
        "gain_normalization": "NONE",
        "automatic_time_alignment": "NONE",
        "original_win95_driver_and_bank_provenance":
            "REQUIRES_SEPARATE_INDEPENDENT_VERIFICATION",
        "source_register_and_zero_FX_parity":
            "REQUIRES_SEPARATE_INDEPENDENT_VERIFICATION",
        "normal_post_CT1745_float_path": "NOT_TESTED_BY_PCM16_COMPARATOR",
        "release_approved": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original-precard-wav", type=Path, required=True)
    parser.add_argument("--vst3-precard-wav", type=Path, required=True)
    parser.add_argument("--original-start-frame", type=int, required=True)
    parser.add_argument("--vst3-start-frame", type=int, required=True)
    parser.add_argument("--frames", type=int, required=True)
    parser.add_argument("--private-report", type=Path, required=True)
    args = parser.parse_args()

    destination = args.private_report.resolve()
    repository = Path(__file__).resolve().parents[1]
    if (destination.exists() or destination == repository
            or repository in destination.parents
            or destination in (args.original_precard_wav.resolve(),
                               args.vst3_precard_wav.resolve())):
        parser.error("Report must be a NEW private file outside Git and input WAVs")
    report = compare_pcm16_windows(
        args.original_precard_wav, args.vst3_precard_wav,
        args.original_start_frame, args.vst3_start_frame, args.frames)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("x", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2, sort_keys=True)
        handle.write("\n")
    print("P002_ORIGINAL_WIN95_VST3_PRECARD_PCM16_"
          + ("EQUAL" if report["sample_exact"] else "MISMATCH"),
          "frames=", report["compared_stereo_frames"],
          "different_samples=", report["mismatched_channel_samples"],
          "max_diff_lsb=", report["max_abs_difference_pcm16_lsb"],
          "original_driver_provenance=NOT_PROVEN_BY_THIS_TOOL",
          "release_approved=FALSE")
    return 0 if report["sample_exact"] else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, wave.Error) as exc:
        print("P002_PRECARD_PCM16_EVIDENCE_NO_GO", exc, file=sys.stderr)
        sys.exit(2)
