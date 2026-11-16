#!/usr/bin/env python3
"""Validate P003 V5 guest-RAM capture without copying private trace bytes.

Each eligible A22 port read must be immediately followed by one V5 comment
whose sequence/TSC/EBX/EBP/caller match the original preserved V4 event.
Recorder coherence is not evidence of x86 timing or audio parity.
"""
from __future__ import annotations
import argparse
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO

HEADER = b'# P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK '

@dataclass(frozen=True)
class Summary:
    ports: int
    eligible_a22_reads: int
    attached_rows: int
    full_ram_snapshots: int
    partial_ram_snapshots: int
    first_sequence: int
    last_sequence: int

def validate(fh: BinaryIO, *, require_full: bool = False) -> Summary:
    if not fh.readline().startswith(HEADER):
        raise ValueError('Original V4_STACK 17-field header is missing')
    ports = eligible = attached = full = partial = 0
    pending: tuple[int, int, int, int, int] | None = None
    previous_sequence = -1
    first_sequence = last_sequence = -1
    for line_number, raw in enumerate(fh, 2):
        tokens = raw.split()
        if not tokens:
            continue
        if tokens[:2] == [b'#', b'P003_V5_ROW_STATE']:
            if pending is None:
                raise ValueError(f'V5 row without preceding eligible A22 read at line {line_number}')
            if len(tokens) != 14:
                raise ValueError(f'V5 row has {len(tokens)} fields at line {line_number}, expected 14')
            try:
                seq, tsc = int(tokens[2]), int(tokens[3])
                ret, ebx, ebp, mask = (int(tokens[j], 16) for j in (4, 5, 6, 7))
                end = int(tokens[8], 16)
                marker = int(tokens[9], 16)
                penalty = int(tokens[10], 16)
                scores = [int(tokens[j], 16) for j in (11, 12, 13)]
            except ValueError as exc:
                raise ValueError(f'V5 data invalid at line {line_number}') from exc
            if (seq, tsc, ret, ebx, ebp) != pending:
                raise ValueError(f'V5 row / preserved V4 port mismatch at line {line_number}')
            if mask & ~0x3f or penalty > 255 or marker > 65535 or end > 0xffffffff or any(v > 0xffffffff for v in scores):
                raise ValueError(f'V5 mask/value out of range at line {line_number}')
            if require_full and mask != 0x3f:
                raise ValueError(f'Unmapped guest RAM: V5 mask={mask:02X} at line {line_number}')
            attached += 1
            if mask == 0x3f:
                full += 1
            else:
                partial += 1
            pending = None
        elif tokens[0] == b'#':
            if pending is not None:
                raise ValueError(f'V5 row missing before line {line_number}')
        else:
            if pending is not None:
                raise ValueError(f'V5 row missing after A22 read at line {line_number}')
            if len(tokens) != 17 or tokens[0] not in (b'R', b'W', b'r'):
                raise ValueError(f'Not a preserved 17-field V4 physical port line at {line_number}')
            try:
                seq = int(tokens[7])
                tsc = int(tokens[1])
                port = int(tokens[5], 16)
                ret = int(tokens[9], 16)
                ebp = int(tokens[8], 16)
                ebx = int(tokens[14], 16)
            except ValueError as exc:
                raise ValueError(f'V4 header/data invalid at line {line_number}') from exc
            if seq != previous_sequence + 1:
                raise ValueError(f'V4 port sequence gap or duplicate at line {line_number}')
            if ports == 0:
                first_sequence = seq
            previous_sequence = last_sequence = seq
            ports += 1
            if tokens[0] == b'R' and port == 0x0a22 and ret != 0xffffffff:
                pending = seq, tsc, ret, ebx, ebp
                eligible += 1
    if pending is not None:
        raise ValueError('Final A22 read is missing its V5 row')
    if not ports or not attached:
        raise ValueError('No preserved V4 port events or no new V5 rows')
    if eligible != attached:
        raise ValueError('V5 rows do not cover all eligible A22 reads')
    return Summary(ports, eligible, attached, full, partial, first_sequence, last_sequence)

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cpu-io-trace', required=True, type=Path)
    parser.add_argument('--require-all-fields', action='store_true')
    args = parser.parse_args()
    with args.cpu_io_trace.open('rb') as fh:
        result = validate(fh, require_full=args.require_all_fields)
    print('P003_V5_GUEST_RAM_COHERENCE_PASS '
          f'port_events={result.ports} attached_row_records={result.attached_rows} '
          f'complete_ram_reads={result.full_ram_snapshots} partial={result.partial_ram_snapshots} '
          'driver_audio_and_x86_parity=NOT_YET_ESTABLISHED')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
