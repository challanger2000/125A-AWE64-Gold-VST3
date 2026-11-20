#!/usr/bin/env python3
"""P019 latency experiment ONLY. Default 980-frame production build untouched.

Provide a test-only compile-time override to the pinned Emu8000Box wrapper.
86Box's original WTBUFLEN remains 980: the experiment flushes only a
shorter prefix, then compares PCM samples after compensating for delay.
No DSP algorithm, Creative source/register math or P007 VM is changed.
Fail closed on any upstream drift, and never enable short windows in the
production VST3 unless ORIGINAL owner audio has been validated.
"""
import pathlib
import sys


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: p019_latency_window_probe.py <pinned-AWE32Emu-checkout>")
    src = pathlib.Path(sys.argv[1]).resolve() / "AWE32Emu" / "src" / "Emu8000Box.h"
    code = src.read_text(encoding="utf-8")
    original = "    static constexpr int kBlockFrames   = 980;   // WTBUFLEN = 44100/45"
    updated = """    // P019: **test-only override**. Default 980 preserves the shipped
    // original 86Box block processing and its known sample timing.
#if !defined(P019_EMU_BLOCK_FRAMES)
#define P019_EMU_BLOCK_FRAMES 980
#endif
    static_assert(P019_EMU_BLOCK_FRAMES >= 1 && P019_EMU_BLOCK_FRAMES <= 980,
                  "P019 block window must fit the original 86Box buffer");
    static constexpr int kBlockFrames   = P019_EMU_BLOCK_FRAMES;"""
    if code.count(original) != 1 or code.count("P019_EMU_BLOCK_FRAMES") != 0:
        raise RuntimeError("Pinned original Emu8000Box.h changed: abort latency probe")
    src.write_text(code.replace(original, updated), encoding="utf-8")
    print("P019_PINNED_86BOX_CHIP_WINDOW_PROBE_980_DEFAULT_UNCHANGED_PASS")


if __name__ == "__main__":
    main()
