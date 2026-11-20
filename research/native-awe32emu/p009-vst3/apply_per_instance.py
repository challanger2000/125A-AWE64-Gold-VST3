#!/usr/bin/env python3
"""P009 native VST3 only: replace global audio-clock target with per-chip state.

Scope is strictly the pinned AWE32Emu checkout fetched by CI. Never changes
86Box VM, original Creative driver binary or P007. Same original DSP math.
Fail closed if any source anchor differs from upstream SHA pin.
"""
from pathlib import Path
import sys

def edit(path: Path, source: str, replacement: str, expected: int = 1):
    text = path.read_text(encoding="utf-8")
    count = text.count(source)
    if count != expected:
        raise RuntimeError(
            f"unsafe pin mismatch: {path}, expected {expected}, got {count}: {source!r}")
    path.write_text(text.replace(source, replacement), encoding="utf-8")

def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_per_instance.py <pinned-upstream-dir>")
    root = Path(sys.argv[1]).resolve() / "AWE32Emu" / "src"
    edit(root / "86box/include/86box/snd_emu8k.h",
         "    int     pos;\n    int32_t buffer[WTBUFLEN * 2];",
         "    int     pos;\n"
         "    int     p009_wavetable_target; /* local instance clock */\n"
         "    int32_t buffer[WTBUFLEN * 2];")
    edit(root / "86box/snd_emu8k.c",
         "wavetable_pos_global", "emu8k->p009_wavetable_target", 7)
    wrapper = root / "Emu8000Box.cpp"
    edit(wrapper, "wavetable_pos_global = w.offset;",
         "I.emu.p009_wavetable_target = w.offset;", 2)
    edit(wrapper, "wavetable_pos_global = I.frameInBlock;",
         "I.emu.p009_wavetable_target = I.frameInBlock;")
    edit(wrapper, "wavetable_pos_global = kBlockFrames;",
         "I.emu.p009_wavetable_target = kBlockFrames;")
    # This path is only used inside emu8k_init(). Make concurrent activation
    # independent without changing the original ROM loader or audio maths.
    edit(wrapper, "    const char* g_romPath = nullptr;",
         "    thread_local const char* g_romPath = nullptr;")
    print("P009_ORIGINAL_PCM_CLOCK_IS_NOW_PER_INSTANCE_NO_VM_PASS")
if __name__ == "__main__":
    main()
