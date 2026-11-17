#!/usr/bin/env python3
"""Run original SBAWE.VXD NoteOn volume x86 against the compiled C++23 port.

Only synthetic channel/descriptor inputs are generated. Original tables and
code are read from the SHA-pinned private driver. Requires Unicorn 2.1.4;
no original bytes, tables or instruction dumps enter repository artifacts.
This is an instruction-block differential, not native VST3/Win95 PCM parity.
"""
from __future__ import annotations
import argparse
import os
import struct
import subprocess
import tempfile
from pathlib import Path
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import (UC_X86_REG_EBP, UC_X86_REG_EDI,
    UC_X86_REG_ESI, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EIP)
from p003_rea_three_object_audit import decode_le

CODE, DESC, VOICE, CHANNEL, DEVICE, FRAME = (
    0x100000, 0x200000, 0x201000, 0x202000, 0x203000, 0x205800)


def corpus():
    # Exhaustive CC7/CC11 at one known descriptor state.
    for cc7 in range(128):
        for cc11 in range(128):
            yield 100, 24, cc7, cc11, 10, 1, 1
    # Every signed generator48 value, including IDIV rounding boundaries.
    for amplitude in range(-32768, 32768):
        yield 100, amplitude, 100, 127, 0, 1, 1
    # Every nonzero MIDI velocity across device/ROM/format branches.
    for velocity in range(1, 128):
        for amplitude in (-32768, -37, -13, -12, 0, 24, 32767):
            for cc7, cc11 in ((0, 127), (127, 10), (127, 11), (64, 64), (127, 127)):
                for format1, rom in ((0, 0), (0, 1), (1, 0), (1, 1)):
                    yield velocity, amplitude, cc7, cc11, 65535, format1, rom


def original_machine(code):
    # Absolute table references are LE fixups, not zero displacements.
    # Driver SHA was checked by decode_le before relocating local memory.
    relocated = bytearray(code)
    for site, target in ((0x5776, 0x2E20), (0x577D, 0x2EA0), (0x57C3, 0x2F20)):
        struct.pack_into('<I', relocated, site, CODE + target)
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    machine.mem_map(CODE, 0x10000)
    machine.mem_write(CODE, bytes(relocated))
    machine.mem_map(DESC, 0x7000)
    machine.mem_write(FRAME - 0x10, struct.pack('<I', CHANNEL))
    return machine


def original_result(machine, case):
    velocity, amplitude, cc7, cc11, additive, format1, rom = case
    machine.mem_write(DESC + 0x5E, struct.pack('<Hh', velocity, amplitude))
    machine.mem_write(CHANNEL + 8, bytes((cc7, cc11)))
    machine.mem_write(VOICE + 0x17, bytes((rom,)))
    machine.mem_write(DEVICE + 0x10, struct.pack('<H', additive))
    machine.mem_write(DEVICE + 0x158E, struct.pack('<I', 0x4D474D31 if format1 else 0))
    for register, value in ((UC_X86_REG_EBP, FRAME), (UC_X86_REG_EDI, DESC),
                            (UC_X86_REG_ESI, VOICE), (UC_X86_REG_EBX, DEVICE),
                            (UC_X86_REG_ECX, CHANNEL)):
        machine.reg_write(register, value)
    machine.emu_start(CODE + 0x5753, CODE + 0x5800, count=128)
    if machine.reg_read(UC_X86_REG_EIP) != CODE + 0x5800:
        raise RuntimeError('Original x86 block did not finish')
    return struct.unpack('<H', machine.mem_read(DESC + 0x26, 2))[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--private-vxd', required=True, type=Path)
    parser.add_argument('--compiled-probe', required=True, type=Path)
    args = parser.parse_args()
    objects, _, _, _ = decode_le(args.private_vxd.read_bytes())
    code = objects[1]
    machine = original_machine(code)
    cases = list(corpus())
    # mkstemp uses mode 0600. Original table bytes remain temporary,
    # outside the checkout, and are removed even if native execution fails.
    fd, name = tempfile.mkstemp(prefix='awe64-private-volume-')
    try:
        with os.fdopen(fd, 'wb') as private:
            private.write(code[0x2E20:0x2FA0])
        result = subprocess.run([str(args.compiled_probe.resolve()), name],
            input=''.join(' '.join(map(str, case)) + '\n' for case in cases),
            capture_output=True, text=True, check=True)
    finally:
        os.unlink(name)
    rows = result.stdout.splitlines()
    if len(rows) != len(cases):
        raise SystemExit('Native probe did not execute the complete corpus')
    mismatches = sum(row != str(original_result(machine, case))
                     for row, case in zip(rows, cases))
    print(f'Original LCOD+0x5753..0x5800 / C++23: cases={len(cases)}, mismatches={mismatches}')
    print('Inputs: synthetic; oracle: executed original x86. VST3/Win95 PCM parity: NOT PROVEN.')
    if mismatches:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
