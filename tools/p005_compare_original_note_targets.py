#!/usr/bin/env python3
"""Execute original x86 target arithmetic against the shared C++23 function.

Private driver bytes/tables stay local. Synthetic arithmetic inputs exercise
original LCOD+0x5B50..0x5B81, +0x5D7B..0x5E5D and +0x5E80..0x5ED5.
Chip-I/O helper calls are outside these regions and are NOT replayed/emulated.
Requires Unicorn 2.1.4; not a Windows VST3/Win95 PCM acceptance test.
"""
from __future__ import annotations
import argparse
import os
import struct
import subprocess
import tempfile
from pathlib import Path
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EDX,
    UC_X86_REG_EBP, UC_X86_REG_ESI, UC_X86_REG_EIP)
from p003_rea_three_object_audit import decode_le

CODE, DESC, VOICE, CHANNEL, FRAME = 0x100000, 0x200000, 0x201000, 0x202000, 0x203800


def corpus():
    for baseline in range(65536):
        yield baseline, 0, 0, 0, 42
    for attenuation in range(-32768, 32768):
        yield 0xE000, 0, 0, 0, attenuation
    for baseline in (0, 0x8000, 0xFFFF):
        for tune in (-32768, -1, 0, 32767):
            for bend in (-2147483648, -1, 0, 1, 2147483647):
                for generator7 in (-32768, -128, 0, 127, 32767):
                    yield baseline, tune, bend, generator7, 20


def execute(machine, case):
    baseline, tune, bend, generator7, attenuation = case
    machine.mem_write(VOICE + 0xE, struct.pack('<H', baseline))
    machine.mem_write(CHANNEL + 0xE, struct.pack('<h', tune))
    machine.mem_write(CHANNEL + 0x14, struct.pack('<i', bend))
    machine.mem_write(FRAME - 0x10, struct.pack('<I', CHANNEL))
    machine.mem_write(DESC + 0xE, struct.pack('<h', generator7))
    machine.mem_write(DESC + 0x26, struct.pack('<h', attenuation))
    for reg, value in ((UC_X86_REG_EAX, CHANNEL), (UC_X86_REG_EDX, VOICE),
                        (UC_X86_REG_EBP, FRAME), (UC_X86_REG_ESI, DESC)):
        machine.reg_write(reg, value)
    def run(start, end):
        machine.emu_start(CODE + start, CODE + end, count=128)
        if machine.reg_read(UC_X86_REG_EIP) != CODE + end:
            raise RuntimeError('Original instruction region did not complete')
    def local(at):
        return struct.unpack('<I', machine.mem_read(FRAME + at, 4))[0]
    run(0x5B50, 0x5B81)
    initial = local(-0xC)
    run(0x5D7B, 0x5E5D)
    target = local(-0x1C)
    # Entry after the attack128 guard and hardware helper: test the actual
    # arithmetic instructions, including negative table indexing and SHRW.
    run(0x5E80, 0x5ED5)
    return initial, target, local(-0x18)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--private-vxd', required=True, type=Path)
    parser.add_argument('--compiled-probe', required=True, type=Path)
    args = parser.parse_args()
    objects, _, _, _ = decode_le(args.private_vxd.read_bytes())
    code = objects[1]
    relocated = bytearray(code)
    struct.pack_into('<I', relocated, 0x5E97, CODE + 0x2FA0)
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    machine.mem_map(CODE, 0x10000)
    machine.mem_write(CODE, bytes(relocated))
    machine.mem_map(DESC, 0x5000)
    cases = list(corpus())
    fd, name = tempfile.mkstemp(prefix='awe64-private-targets-')
    try:
        with os.fdopen(fd, 'wb') as private:
            private.write(code[0x2F82:0x2FC0])
        native = subprocess.run([str(args.compiled_probe.resolve()), name],
            input=''.join(' '.join(map(str, case)) + '\n' for case in cases),
            capture_output=True, text=True, check=True)
    finally:
        os.unlink(name)
    rows = native.stdout.splitlines()
    if len(rows) != len(cases):
        raise SystemExit('Native target probe did not execute the full corpus')
    mismatches = sum(tuple(map(int, row.split())) != execute(machine, case)
                     for row, case in zip(rows, cases))
    print(f'Original x86 / shared C++23 targets: cases={len(cases)}, mismatches={mismatches}')
    print('Synthetic inputs; actual original arithmetic blocks. VST3/Win95 PCM parity: NOT PROVEN.')
    if mismatches:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
