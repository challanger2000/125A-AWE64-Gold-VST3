#!/usr/bin/env python3
"""Compare 980/128/64-window real EMU8000 output after exact FIFO alignment.
Synthetic ROM + public SF2 only. DIFFERENCES ARE NO-GO for production.
"""
from array import array
import hashlib
import json
import math
from pathlib import Path
import sys


def read_pcm(path):
    blob = Path(path).read_bytes()
    if not blob or len(blob) % 4:
        raise ValueError(f"invalid raw stereo PCM {path}")
    a = array("h")
    a.frombytes(blob)
    if sys.byteorder != "little":
        a.byteswap()
    return a


def compare(ref, candidate, window, mode):
    assert window in (128, 64)
    # The new window changes only the number of queued 44.1k frames.
    # Align the exact one-window 980 vs short latency before comparison.
    a = ref[980 * 2:]
    b = candidate[window * 2:]
    count = min(len(a), len(b))
    a, b = a[:count], b[:count]
    if count < 100000:
        raise ValueError("PCM comparison too short")
    signal = sum(x*x for x in a)
    if signal <= 100000000:
        raise ValueError("reference signal silent")
    sum_error = 0
    peak_error = 0
    mismatches = 0
    for x, y in zip(a, b):
        d = x-y
        if d:
            mismatches += 1
            peak_error = max(peak_error, abs(d))
            sum_error += d*d
    rms_error = math.sqrt(sum_error / count)
    rms_reference = math.sqrt(signal / count)
    exact = mismatches == 0
    return dict(
        mode=mode,
        experiment_window_frames=window,
        reference_window_frames=980,
        sample_rate_hz=44100,
        reference_latency_ms=980/44.1,
        candidate_latency_ms=window/44.1,
        compared_stereo_samples=count,
        mismatch_samples=mismatches,
        exact_fraction=(count-mismatches)/count,
        max_absolute_error_lsb=peak_error,
        rms_error_lsb=rms_error,
        relative_rms_error_db=(20*math.log10(rms_error/rms_reference)
                               if rms_error else None),
        bit_identical=exact,
        production_approved=False,  # private original-ROM still mandatory
        original_rom_or_creative_hw_reference=False,
    )


def main():
    if len(sys.argv) != 9:
        raise SystemExit(
            "usage: p019_compare_pcm.py dry-980.pcm dry-128.pcm dry-64.pcm "
            "fx-980.pcm fx-128.pcm fx-64.pcm report.json report.txt")
    traces = {
        mode: {
            n: read_pcm(p) for n, p in zip((980, 128, 64), sys.argv[b:b+3])
        }
        for mode, b in (("dry", 1), ("fx", 4))
    }
    cases = [
        compare(traces[mode][980], traces[mode][n], n, mode)
        for mode in ("dry", "fx") for n in (128, 64)
    ]
    data = {
        "test": "P019 original chip shorter window PCM parity experiment",
        "origin": "pinned original 86Box-derived chip with generated public two-layer SF2",
        "private_original_assets_used": False,
        "shipping_vst3_render_window_frames": 980,
        "result": ("SHORT_WINDOWS_PCM_BIT_EXACT_IN_PUBLIC_FIXTURE"
                   if all(x["bit_identical"] for x in cases)
                   else "SHORT_WINDOWS_CHANGE_ORIGINAL_CHIP_PCM_NO_RELEASE"),
        "cases": cases,
    }
    Path(sys.argv[7]).write_text(json.dumps(data, indent=2)+"\n", encoding="utf-8")
    text = "\n".join(
        (f"P019_{case['mode'].upper()}_WINDOW{case['experiment_window_frames']}"
         f"_PCM={'BIT_EXACT' if case['bit_identical'] else 'DIFFERENT'}"
         f" mismatches={case['mismatch_samples']}/{case['compared_stereo_samples']}"
         f" max_lsb={case['max_absolute_error_lsb']}"
         f" rms_error_lsb={case['rms_error_lsb']:.6f}"
         f" rel_db={case['relative_rms_error_db']}") for case in cases)
    text += "\nP019_PRODUCTION_DEFAULT_980_FRAMES_UNCHANGED\n"
    text += "P019_DO_NOT_TREAT_PUBLIC_FIXTURE_AS_ORIGINAL_CREATIVE_ROM_ACCEPTANCE\n"
    Path(sys.argv[8]).write_text(text, encoding="utf-8")
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
