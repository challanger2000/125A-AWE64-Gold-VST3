#!/usr/bin/env python3
"""P193 diagnostic: public SoundFont 2.04 A/B with omitted/explicit default attack.

The two RIFF files have identical sample data, regions, loops and other
parameters.  Gen 34 absent and gen 34=-12000 timecents are semantically
equivalent according to SoundFont 2.04 section 8.1.3.  No owner data.
"""
import math
import struct
import sys
from pathlib import Path

def ck(tag, data):
    assert len(tag) == 4
    return tag + struct.pack("<I", len(data)) + data + (b"\0" if len(data) % 2 else b"")

def lst(tag, data):
    return ck(b"LIST", tag + data)

def name(s):
    return s.encode("ascii")[:20].ljust(20, b"\0")

def phdr(n, program, bank, bag):
    return struct.pack("<20sHHHIII", name(n), program, bank, bag, 0, 0, 0)

def inst(n, bag):
    return struct.pack("<20sH", name(n), bag)

def bag(n):
    return struct.pack("<HH", n, 0)

def gen(op, value):
    return struct.pack("<HH", op, value & 65535)

def shdr(n, start, end, loop_start, loop_end):
    return struct.pack("<20sIIIIIBbHH", name(n), start, end, loop_start,
                       loop_end, 44100, 60, 0, 0, 1)

def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: P193AttackDefaultPair.py <output-directory>")
    dest = Path(sys.argv[1])
    dest.mkdir(parents=True, exist_ok=True)
    count = 44100
    smpl = struct.pack("<%dh" % count, *(
        int(12000 * math.sin(2 * math.pi * (i % 220) / 220))
        for i in range(count)))
    info = (ck(b"ifil", struct.pack("<HH", 2, 4)) +
            ck(b"INAM", b"P193 Public Attack Default A/B\0\0"))
    common = (
        ck(b"phdr", phdr("DefaultAttack", 0, 0, 0) + phdr("EOP", 0, 0, 1)) +
        ck(b"pbag", bag(0) + bag(1)) + ck(b"pmod", bytes(10)) +
        ck(b"pgen", gen(41, 0) + gen(0, 0)) +
        ck(b"inst", inst("Sine", 0) + inst("EOI", 1)))
    for explicit in (False, True):
        layers = (([gen(34, -12000)] if explicit else []) +
                  [gen(37, 0), gen(54, 1), gen(53, 0)])
        pdta = (common + ck(b"ibag", bag(0) + bag(len(layers))) +
                ck(b"imod", bytes(10)) +
                ck(b"igen", b"".join(layers) + gen(0, 0)) +
                ck(b"shdr", shdr("Sine", 0, count, 240, count - 240) +
                   shdr("EOS", count, count, count, count)))
        data = ck(b"RIFF", b"sfbk" + lst(b"INFO", info) +
                  lst(b"sdta", ck(b"smpl", smpl)) + lst(b"pdta", pdta))
        dst = dest / ("p193-attack-explicit.sf2" if explicit
                      else "p193-attack-omitted.sf2")
        dst.write_bytes(data)
        print("P193_PUBLIC_ATTACK_PAIR", dst.name, len(data))
if __name__ == "__main__":
    main()
