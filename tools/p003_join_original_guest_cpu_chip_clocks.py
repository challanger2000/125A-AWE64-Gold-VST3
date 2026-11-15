#!/usr/bin/env python3
"""P003: strict original 86Box EMU8K sample frame <-> guest CPU I/O join.

The original chip and private Win95 Creative driver are NEVER modified.
An instrumented *86Box io.c* records CPU TSC/cycles and CS:EIP for the six
EMU8000 I/O ports, while the unchanged EMU8K_TRACE records 44.1kHz frames.
Match ALL direction/port/value events in original order or FAIL CLOSED.
TSC and chip-frame alignment is merely observed, not interpolated,
normalized, resampled, or claimed to represent reliable x86 timing yet.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import itertools
import json
from pathlib import Path
import sys

PORTS = {0xe22, 0xe20, 0xa20, 0xa22, 0x620, 0x622}
SECTIONS = ("C4_ON", "C4_OFF", "D4_ON", "D4_OFF")

def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024*1024), b""):
            h.update(chunk)
    return h.hexdigest()

def chip_events(lines):
    first = next(lines, "").rstrip("\r\n")
    if first != "# EMU8000 port write trace, 44100 Hz timebase":
        raise ValueError("Wrong/non-original EMU8K_TRACE header")
    last_frame = -1
    for number, line in enumerate(lines, 2):
        if not line.strip() or line.startswith("#"):
            continue
        tokens = line.split()
        if len(tokens) == 4 and tokens[0] == "R":
            direction, frame, port, word = tokens
        elif len(tokens) == 3:
            direction = "W"
            frame, port, word = tokens
        else:
            raise ValueError(f"Invalid chip source event at line {number}")
        try:
            f = int(frame, 10)
            p = int(port, 16)
            w = int(word, 16)
        except ValueError as exc:
            raise ValueError(f"Invalid chip numeric event at line {number}") from exc
        if f < last_frame or f < 0 or p not in PORTS or not 0 <= w <= 65535:
            raise ValueError(f"Chip event order/port/value invalid at line {number}")
        last_frame = f
        yield (direction, f, p, w)

def guest_events(lines):
    first = next(lines, "").rstrip("\r\n")
    if not first.startswith("# P003_CPU_GUEST_OUTW_V1 "):
        raise ValueError("Wrong guest x86 I/O trace header")
    expected_seq = 0
    for number, line in enumerate(lines, 2):
        if not line.strip() or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) != 8:
            raise ValueError(f"Guest x86 I/O event has wrong width at line {number}")
        try:
            direction = parts[0]
            tsc = int(parts[1], 10)
            remaining_cycles = int(parts[2], 10)
            cs, eip, port, value = (int(v, 16) for v in parts[3:7])
            seq = int(parts[7], 10)
        except ValueError as exc:
            raise ValueError(f"Bad guest x86 CPU field at line {number}") from exc
        if (direction not in ("R", "W") or tsc < 0 or
            port not in PORTS or not 0 <= value <= 65535 or
            not 0 <= cs <= 65535 or not 0 <= eip <= 0xffffffff or
            seq != expected_seq):
            raise ValueError(f"Guest CPU event invalid/out of order at line {number}")
        expected_seq += 1
        yield (direction, port, value, tsc, remaining_cycles, cs, eip)

def note_fixture(lines):
    groups: dict[str, list[tuple[int,int,int]]] = {}
    section = None
    for lineno, line in enumerate(lines, 1):
        line = line.strip()
        if line.startswith("# "):
            title = line[2:]
            if title in SECTIONS:
                if title in groups or (
                        section is None and title != SECTIONS[0]) or (
                        section is not None and
                        SECTIONS.index(title) != SECTIONS.index(section) + 1):
                    raise ValueError("Wrong note group order/duplicate")
                section = title
                groups[title] = []
        elif line and not line.startswith("#"):
            if section is None:
                raise ValueError("Note group header missing")
            parts = line.split()
            if len(parts) != 3:
                raise ValueError(f"Wrong note port event at {lineno}")
            frame = int(parts[0]); port = int(parts[1],16); value = int(parts[2],16)
            if (port not in PORTS or not 0 <= value <= 65535 or
                    (groups[section] and frame < groups[section][-1][0])):
                raise ValueError(f"Invalid note event at {lineno}")
            groups[section].append((frame,port,value))
    if tuple(groups) != SECTIONS or any(not groups[x] for x in SECTIONS):
        raise ValueError("Incomplete original note event fixture")
    return groups

def join_traces(chip_lines, cpu_lines, original_notes=None):
    groups = original_notes or {}
    found = {name: [] for name in groups}
    seen = 0
    reg_count = 0
    tsc_regressions = 0
    old_tsc = None
    first_tsc = None
    last_tsc = None
    first_frame = None
    last_frame = None
    for item in itertools.zip_longest(chip_events(iter(chip_lines)),
                                      guest_events(iter(cpu_lines))):
        chip, cpu = item
        if chip is None or cpu is None:
            raise ValueError(f"Original chip and CPU I/O trace count differs after {seen} events")
        direction, frame, port, value = chip
        cd, cp, cv, tsc, cycles, cs, eip = cpu
        if (direction,port,value) != (cd,cp,cv):
            raise ValueError(f"Original chip/guest CPU register mismatch at event {seen}")
        seen += 1
        if old_tsc is not None and tsc < old_tsc:
            tsc_regressions += 1
        old_tsc = tsc
        if first_tsc is None:
            first_tsc = tsc
            first_frame = frame
        last_tsc = tsc
        last_frame = frame
        if direction == "W":
            reg_count += 1
            for name, expected in groups.items():
                if expected[0][0] <= frame <= expected[-1][0]:
                    found[name].append({
                        "frame": frame, "port": port, "value": value,
                        "tsc": tsc, "cycles": cycles,
                        "cs": cs, "eip": eip})
    if seen == 0:
        raise ValueError("Original chip/CPU I/O traces are empty")
    result = {
        "schema": "125A-P003-ORIGINAL-CHIP-CPU-CLOCK-JOIN-V1",
        "matched_io_events": seen,
        "physical_write_events": reg_count,
        "first_chip_frame": first_frame,
        "last_chip_frame": last_frame,
        "first_guest_tsc": first_tsc,
        "last_guest_tsc": last_tsc,
        "guest_tsc_regressions": tsc_regressions,
        "event_sequence_exact": True,
        "guest_cpu_to_chip_clock_calibrated": False,
        "original_emu8000_modified": False,
        "note_groups": {}
    }
    for name, expected in groups.items():
        observed = found[name]
        if [(x["frame"],x["port"],x["value"]) for x in observed] != expected:
            raise ValueError(f"Original note {name} did not match exact SHA source OUTs")
        eips = Counter(f'{x["cs"]:04X}:{x["eip"]:08X}' for x in observed)
        tscs = [x["tsc"] for x in observed]
        result["note_groups"][name] = {
            "original_register_writes": len(observed),
            "start_chip_frame": expected[0][0],
            "end_chip_frame": expected[-1][0],
            "start_guest_tsc": tscs[0],
            "end_guest_tsc": tscs[-1],
            "guest_tsc_delta": tscs[-1]-tscs[0],
            "guest_x86_call_sites": dict(eips),
            "first_8_port_tsc": tscs[:8],
            "last_8_port_tsc": tscs[-8:],
            "guest_cpu_remaining_cycles_first_last":
                [observed[0]["cycles"],observed[-1]["cycles"]]
        }
    return result

def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--original-chip-trace",required=True,type=Path)
    p.add_argument("--guest-cpu-io-trace",required=True,type=Path)
    p.add_argument("--original-note-ports",type=Path)
    p.add_argument("--chip-sha256",help="Original run owner-attested chip SHA256")
    p.add_argument("--note-ports-sha256",help="Original owner note fixture SHA256")
    p.add_argument("--out-json",required=True,type=Path)
    args = p.parse_args()
    try:
        chip_sha = sha256(args.original_chip_trace)
        cpu_sha = sha256(args.guest_cpu_io_trace)
        if args.chip_sha256 and chip_sha != args.chip_sha256:
            raise ValueError("Original owner chip SHA256 disagreement")
        if bool(args.original_note_ports) != bool(args.note_ports_sha256):
            raise ValueError("Attested original note fixture and SHA256 both required")
        notes = None
        if args.original_note_ports:
            if sha256(args.original_note_ports) != args.note_ports_sha256:
                raise ValueError("Source original note-ports SHA256 disagreement")
            with args.original_note_ports.open(encoding="utf-8") as f:
                notes=note_fixture(f)
        with args.original_chip_trace.open(encoding="utf-8") as chip, \
             args.guest_cpu_io_trace.open(encoding="utf-8") as cpu:
            report=join_traces(chip,cpu,notes)
        report["original_chip_trace_sha256"]=chip_sha
        report["instrumented_guest_cpu_trace_sha256"]=cpu_sha
        report["original_chip_sha256_verified"]=bool(args.chip_sha256)
        report["original_note_sha256_verified"]=bool(notes)
        if args.out_json.exists():
            raise ValueError("Refuse overwrite of previous owner timing report")
        with args.out_json.open("x",encoding="utf-8") as out:
            json.dump(report,out,indent=2,sort_keys=True)
            out.write("\n")
        print("P003_ORIGINAL_GUEST_CPU_AND_CHIP_PORT_SEQUENCE_MATCHED "
              f"events={report['matched_io_events']} "
              "subsample_scheduler_parity=NOT_YET_DERIVED")
        return 0
    except (ValueError,OSError,UnicodeError) as exc:
        print("P003_ORIGINAL_CPU_CHIP_CLOCK_JOIN_NO_GO: "+str(exc),file=sys.stderr)
        return 2

if __name__=="__main__":
    sys.exit(main())
