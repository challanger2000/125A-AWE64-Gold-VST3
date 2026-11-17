#!/usr/bin/env python3
"""Execute private original x86 branches against the compiled C++23 port.

Requires Unicorn 2.1.4 and the SHA-pinned owner SBAWE.VXD, supplied locally.
Never writes driver bytes, disassembly, ROMs, traces or sample data. The
inputs are synthetic descriptor cases; the oracle EXECUTES original x86,
not a transcribed arithmetic model. This is not VST3/Win95 PCM acceptance.
"""
from __future__ import annotations
import argparse
import random
import struct
import subprocess
from pathlib import Path
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_EDI, UC_X86_REG_ESI
from p003_rea_three_object_audit import decode_le

CODE, DESC, VOICE, CHANNEL, STACK = 0x100000, 0x200000, 0x201000, 0x202000, 0x203000


def original_oracle(code: bytes):
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    machine.mem_map(CODE, 0x10000)
    machine.mem_write(CODE, code)
    machine.mem_map(DESC, 0x5000)
    return machine


def execute(machine, case):
    mode, start, end, ls, le, key, root, pitch, corr, fine, coarse, scale, t1, t2 = case
    desc = bytearray(148)
    for at, value in ((0x6C, mode), (0x5C, key), (0x74, root),
                      (0x68, fine), (0x66, coarse), (0x70, scale)):
        struct.pack_into('<H', desc, at, value & 0xFFFF)
    for at, value in ((0x76, start), (0x7A, end), (0x7E, ls), (0x82, le), (0x86, pitch)):
        struct.pack_into('<I', desc, at, value)
    desc[0x8A], desc[0x8B] = 60, corr & 255
    machine.mem_write(DESC, bytes(desc))
    machine.mem_write(VOICE, bytes(32))
    channel = bytearray(0x20)
    struct.pack_into('<hh', channel, 0x10, t1, t2)
    machine.mem_write(CHANNEL, bytes(channel))
    frame = STACK + 0x800
    machine.mem_write(frame - 0x10, struct.pack('<I', CHANNEL))
    machine.reg_write(UC_X86_REG_EBP, frame)
    machine.reg_write(UC_X86_REG_EDI, DESC)
    machine.reg_write(UC_X86_REG_ESI, VOICE)
    # No calls or relocations occur in either bounded source region.
    machine.emu_start(CODE + 0x56CE, CODE + 0x5753, count=128)
    ip = struct.unpack('<H', machine.mem_read(VOICE + 0xE, 2))[0]
    machine.mem_write(frame - 0x20, struct.pack('<I', VOICE))
    machine.reg_write(UC_X86_REG_ESI, DESC)
    machine.emu_start(CODE + 0x5B81, CODE + 0x5BE4, count=64)
    read = lambda at: struct.unpack('<I', machine.mem_read(frame + at, 4))[0]
    return ip, read(-0x24), read(-0x38), read(-0x34)


def cases():
    rng = random.Random(0x125A)
    for ram in (False, True):
        for mode in range(4):
            for i in range(512):
                start = (0x200000 if ram else 0) + 0x100 + i * 16
                ls, le = start + 16, start + 64
                if mode & 1 == 0 and i & 1:
                    ls, le = 0xFFFFFFFF, 0  # Original never reads these.
                yield (mode, start, start + 80, ls, le, i % 128,
                       0xFFFF if i & 1 else rng.randrange(128),
                       rng.randrange(0x1000000), rng.randrange(-128, 128),
                       rng.randrange(-32768, 32768), rng.randrange(-32768, 32768),
                       rng.randrange(-32768, 32768), rng.randrange(-32768, 32768),
                       rng.randrange(-32768, 32768))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--private-vxd', required=True, type=Path)
    parser.add_argument('--compiled-probe', required=True, type=Path)
    args = parser.parse_args()
    objects, _, _, _ = decode_le(args.private_vxd.read_bytes())
    machine = original_oracle(objects[1])
    corpus = list(cases())
    result = subprocess.run([str(args.compiled_probe.resolve())],
        input=''.join(' '.join(map(str, c)) + '\n' for c in corpus),
        text=True, capture_output=True, check=True)
    rows = result.stdout.splitlines()
    if len(rows) != len(corpus):
        raise SystemExit('Native probe did not execute the full corpus')
    mismatches = 0
    for case, row in zip(corpus, rows):
        expected = execute(machine, case)
        actual = tuple(map(int, row.split())) if row != 'REJECT' else None
        mismatches += actual != expected
    print(f'Original x86 / compiled C++23: cases={len(corpus)}, mismatches={mismatches}')
    print('Scope: synthetic descriptor inputs; original LCOD+0x56CE..0x5753 '
          'and +0x5B81..0x5BE4 executed. VST3/Win95 PCM parity: NOT PROVEN.')
    if mismatches:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
