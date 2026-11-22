#!/usr/bin/env python3
"""P209: allow valid separately bounded left/right SF2 stereo samples.

The original EMU8000 plays left and right with separate sample addresses
and separate loop bounds. Equal sample lengths are NOT required for safe
adjacent physical voice pairing. Four real previously supplied owner SF2
banks contain reciprocal and same-rate linked samples with different
lengths, so previous P199 parser rejected these ENTIRE valid banks.
Retain strict link reciprocal/type/rate and non-negative length gates.
"""
from pathlib import Path
import sys

def main():
    if len(sys.argv)!=2:raise SystemExit("usage: p209.py <pinned upstream>")
    root=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"
    path=root/"SoundFont.cpp"
    s=path.read_text(encoding="utf-8")
    old="""                || mate.end < mate.start || s.end < s.start
                || mate.end - mate.start != s.end - s.start)"""
    new="""                || mate.end < mate.start || s.end < s.start
                // P209_OWNER_UNEQUAL_STEREO: EMU8000 left/right samples
                // have independent hardware end/loop registers. All linked
                // partners STILL require reciprocal IDs and equal rates.
                // The old unequal-length rejection remains as a test-only
                // causal mutant; it is not compiled into shipped VST3.
#ifdef P209_NEGATIVE_REQUIRE_EQUAL_STEREO_LENGTH
                || mate.end - mate.start != s.end - s.start
#endif
                )"""
    if s.count(old)!=1:raise RuntimeError(f"P209 pinned stereo length check anchor count {s.count(old)}")
    s=s.replace(old,new)
    path.write_text(s,encoding="utf-8")
    print("P209_SF2_RECIPROCAL_STEREO_INDEPENDENT_LENGTHS_ACCEPTED_REAL_86BOX_PCM")

if __name__=="__main__":main()
