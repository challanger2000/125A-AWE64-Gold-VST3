#!/usr/bin/env python3
"""Strictly test-only resurrection of the historical unsafe voice0 fallback.
Production compiles the safe no-candidate path. Both variants use the same
pinned genuine 86Box EMU8000 sources; chip DSP remains unchanged.
"""
import sys
from pathlib import Path

def main():
    if len(sys.argv)!=2:raise SystemExit('p210_patch.py <pinned upstream>')
    path=Path(sys.argv[1]).resolve()/'AWE32Emu'/'src'/'Synth.cpp'
    src=path.read_text(encoding='utf-8')
    old='    if (chosen < 0) return -1;'
    new='''    // P210_32_VOICE_BUDGET. Never select a voice reserved by this NoteOn.
    // Negative-only standalone QA must resurrect old voice0 aliasing.
#ifdef P210_NEGATIVE_REUSE_RESERVED_VOICE0
    if (chosen < 0) chosen = 0;
#else
    if (chosen < 0) return -1;
#endif'''
    if src.count(old)!=1:
        raise RuntimeError('P210 pinned safe no-candidate anchor count '+str(src.count(old)))
    path.write_text(src.replace(old,new),encoding='utf-8')
    print('P210_REAL_32_VOICE_ALLOCATOR_FAIL_CLOSED_SOURCE_PATCH_APPLIED')
if __name__=='__main__':main()
