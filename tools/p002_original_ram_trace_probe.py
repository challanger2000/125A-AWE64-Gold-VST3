#!/usr/bin/env python3
"""P002 PRIVATE original-Creative Win95 SMALW/SMLD RAM-upload trace probe.

Reads a caller-provided 86Box EMU8K_TRACE stream and a private standard
SF2 bank; finds the full, exact 16-bit mono sample bytes in the original
EMU8000 RAM port write stream, and reports the HARDWARE WORD address that
the captured register traffic (not our own guessed allocator) used.

Strictly a SOURCE-TRACE/PCM ALIGNMENT RESEARCH GATE. This cannot prove
the trace really came from Creative's Win95 driver: provenance must be
checked by a separate Win95 original-SBAWE.VXD capture. It cannot prove
native Creative sample upload, allocate voices, produce a shippable
soundbank, or certify original-driver PCM parity.

Usage (private assets MUST stay outside Git):
 python tools/p002_original_ram_trace_probe.py \
   --original-emu8k-trace PRIVATE/emu8k-original-bank-upload.trace \
   --private-sf2 PRIVATE/SynthBank.sf2 \
   --sample-id 0 --private-report PRIVATE/ram-probe.json
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

try:
    from .p002_register_oracle import TRACE
except ImportError:
    from p002_register_oracle import TRACE

RAM_FIRST = 0x200000
RAM_END_EXCLUSIVE = 0xFFFFE0
MAX_RAM_WORDS = RAM_END_EXCLUSIVE - RAM_FIRST
MAX_TRACE_BYTES = 1_200_000_000
MAX_TRACE_LINES = 32_000_000
MAX_CLOCK_FRAME = 44100 * 4 * 3600


def chunks(data: bytes, first: int, end: int):
    if first > end or end > len(data):
        raise ValueError("RIFF container range invalid")
    pos = first
    while pos < end:
        if end - pos < 8:
            raise ValueError("RIFF truncated child header")
        size = struct.unpack_from("<I", data, pos + 4)[0]
        payload = pos + 8
        if size > end - payload:
            raise ValueError("RIFF child exceeds container")
        yield data[pos:pos + 4], payload, size
        pos = payload + size + (size & 1)
        if pos > end:
            raise ValueError("RIFF child odd padding exceeds parent")


def unique(items, tag):
    found = [(start, size) for name, start, size in items if name == tag]
    if len(found) != 1:
        raise ValueError("SoundFont needs exactly one " + tag.decode("ascii"))
    return found[0]


def extract_sf2_mono_sample(raw: bytes, sample_id: int):
    if len(raw) < 12 or raw[:4] != b"RIFF" or raw[8:12] != b"sfbk":
        raise ValueError("not a standard RIFF SoundFont")
    if struct.unpack_from("<I", raw, 4)[0] != len(raw) - 8:
        raise ValueError("incorrect full SF2 RIFF length")
    top = [(tag, at, n) for tag, at, n in chunks(raw, 12, len(raw))
           if tag == b"LIST" and n >= 4]
    info = [x for x in top if raw[x[1]:x[1] + 4] == b"INFO"]
    sdta = [x for x in top if raw[x[1]:x[1] + 4] == b"sdta"]
    pdta = [x for x in top if raw[x[1]:x[1] + 4] == b"pdta"]
    if len(info) != 1 or len(sdta) != 1 or len(pdta) != 1:
        raise ValueError("missing or duplicate INFO/sdta/pdta LIST")
    a = [(tag, at, n) for tag, at, n
         in chunks(raw, info[0][1] + 4, info[0][1] + info[0][2])]
    pos, size = unique(a, b"ifil")
    if size != 4 or struct.unpack_from("<H", raw, pos)[0] != 2:
        raise ValueError("not a format-2 SoundFont")
    s = [(tag, at, n) for tag, at, n
         in chunks(raw, sdta[0][1] + 4, sdta[0][1] + sdta[0][2])]
    if any(tag == b"sm24" for tag, _, _ in s):
        raise ValueError("24-bit supplemental SF2 PCM unsupported by source gate")
    sample_offset, sample_len = unique(s, b"smpl")
    if sample_len < 2 or sample_len % 2:
        raise ValueError("missing or odd-length smpl data")
    p = [(tag, at, n) for tag, at, n
         in chunks(raw, pdta[0][1] + 4, pdta[0][1] + pdta[0][2])]
    hdr_start, hdr_len = unique(p, b"shdr")
    if hdr_len % 46 or hdr_len < 92:
        raise ValueError("standard 46-byte SoundFont sample headers missing")
    count = hdr_len // 46
    if not 0 <= sample_id < count - 1:
        raise ValueError("sample ID refers to EOS or out of bounds")
    hdr = hdr_start + 46 * sample_id
    first, end, loop_first, loop_end, rate = struct.unpack_from(
        "<IIIII", raw, hdr + 20)
    root, correction = struct.unpack_from("<Bb", raw, hdr + 40)
    link, sample_type = struct.unpack_from("<HH", raw, hdr + 42)
    if (sample_type != 1 or link != 0 or root > 127 or rate == 0 or
            first >= end or end > sample_len // 2 or
            not first <= loop_first < loop_end <= end or
            end - first > MAX_RAM_WORDS):
        raise ValueError("unsupported or invalid original RAM mono sample")
    pcm = raw[sample_offset + 2 * first: sample_offset + 2 * end]
    if len(pcm) < 32 or not any(pcm):
        raise ValueError("insufficient/non-audible PCM for original trace match")
    return dict(sample_id=sample_id, source_start_word=first,
                source_end_word=end, source_loop_start=loop_first,
                source_loop_end=loop_end, sample_rate=rate,
                original_root_pitch=root, correction=correction,
                data=pcm, pcm16_sha256=hashlib.sha256(pcm).hexdigest())


def scan_capture(lines, expected_pcm: bytes):
    """Stream exact 86Box original-register text; do not invent RAM addresses.

    Keep only one contiguous physical DRAM word run (at most 28 MiB).
    A direct full-SF2-sample byte match can be found even if the driver
    writes other samples before/after it, or repoints SMALW to the next
    contiguous physical address. Never infer missing writes.
    """
    selector = None
    lo = hi = None
    pointer = None
    frame_prev = -1
    line_count = write_words = read_events = matches = 0
    first_match_address = None
    run_address = None
    run = bytearray()

    def evaluate():
        nonlocal matches, first_match_address
        if run_address is None:
            return
        # A sample may appear more than once in the SAME original
        # contiguous RAM upload. Do not silently accept the first
        # occurrence and call an ambiguous address Creative-verified.
        at = run.find(expected_pcm)
        while at >= 0:
            if at % 2 == 0:
                matches += 1
                if first_match_address is None:
                    first_match_address = run_address + at // 2
                # A second physical hit is an ambiguous original
                # placement. Only one unique hit is admissible.
                if matches > 1:
                    return
            at = run.find(expected_pcm,at+2)

    for lineno, raw in enumerate(lines, 1):
        if lineno > MAX_TRACE_LINES:
            raise ValueError("original EMU8K capture exceeds event limit")
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        m = TRACE.fullmatch(line)
        if not m:
            raise ValueError(f"invalid original EMU8K register event at line {lineno}")
        read, frame_raw, port_raw, value_raw = m.groups()
        frame, port, value = int(frame_raw), int(port_raw, 16), int(value_raw, 16)
        if frame < frame_prev or frame > MAX_CLOCK_FRAME:
            raise ValueError(f"nonmonotonic original chip clock at line {lineno}")
        frame_prev = frame
        line_count += 1
        if read:
            read_events += 1
            continue
        if port == 0xE22:
            selector = value
            if selector == 0x36:  # exactly original EMU8000 SMALW
                pointer = None
                lo = hi = None
            continue
        if selector == 0x36:
            if port == 0xA20:
                if lo is not None:
                    raise ValueError("duplicate original SMALW low address")
                lo = value
            elif port == 0xA22:
                if hi is not None:
                    raise ValueError("duplicate original SMALW high address")
                hi = value
            if lo is not None and hi is not None:
                pointer = lo | (hi << 16)
                if not RAM_FIRST <= pointer < RAM_END_EXCLUSIVE:
                    raise ValueError("captured Creative RAM pointer outside installed 28 MiB")
        elif selector == 0x3A and port == 0xA20:
            if pointer is None or not RAM_FIRST <= pointer < RAM_END_EXCLUSIVE:
                raise ValueError("original SMLD write missing safe SMALW pointer")
            if run_address is None:
                run_address = pointer
            elif pointer != run_address + len(run) // 2:
                evaluate()
                run.clear()
                run_address = pointer
            run.extend(struct.pack("<H", value))
            pointer += 1
            write_words += 1
            if len(run) > MAX_RAM_WORDS * 2:
                raise ValueError("original contiguous RAM upload exceeds installed capacity")
    evaluate()
    if write_words == 0:
        raise ValueError("original trace has no EMU8000 SMALW/SMLD RAM writes")
    return dict(register_events=line_count, original_register_reads=read_events,
                ram_word_writes=write_words,
                direct_pcm16_upload_matches=matches,
                first_match_emu8000_word_address=first_match_address)


def filter_original_ram_upload_port_writes(lines):
    """Extract only ACTUAL captured SMALW/SMLD writes.

    Each emitted 3-field event is a literal write from the original
    EMU8K_TRACE (frame, port, value). No new pointer, sample, blank loop
    padding, source byte order or address is ever synthesized. The
    filtered output is directly compatible with the original chip
    port-replay preflight, not a general register-trace equivalent.
    """
    selector = None
    low = high = None
    pointer = None
    frame_prev = -1
    yielded_words = 0
    for number, raw in enumerate(lines, 1):
        if number > MAX_TRACE_LINES:
            raise ValueError("Original trace exceeds RAM capture event limit")
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        m = TRACE.fullmatch(line)
        if not m:
            raise ValueError(f"Invalid original captured register line {number}")
        read, frame_raw, port_raw, value_raw = m.groups()
        frame = int(frame_raw)
        port = int(port_raw, 16)
        value = int(value_raw, 16)
        if frame < frame_prev or frame > MAX_CLOCK_FRAME:
            raise ValueError(f"Original RAM capture clock order at line {number}")
        frame_prev = frame
        if read:
            continue
        if port == 0xE22:
            if value == 0x36:
                if selector == 0x36 and (low is None or high is None):
                    raise ValueError("Original RAM pointer truncated mid-capture")
                selector = 0x36
                low = high = None
                pointer = None
                yield frame, port, value
            elif value == 0x3A:
                if pointer is None:
                    # Captures started mid-stream without a valid
                    # original SMALW pointer are NOT self-contained.
                    raise ValueError("Original SMLD stream has no SMALW pointer")
                selector = 0x3A
                yield frame, port, value
            else:
                if selector == 0x36 and (low is None or high is None):
                    raise ValueError("Incomplete original RAM address setter")
                selector = None
            continue
        if selector == 0x36 and port in (0xA20, 0xA22):
            if port == 0xA20:
                if low is not None:
                    raise ValueError("Duplicate original RAM low pointer")
                low = value
            else:
                if high is not None:
                    raise ValueError("Duplicate original RAM high pointer")
                high = value
            yield frame, port, value
            if low is not None and high is not None:
                pointer = low | (high << 16)
                if not RAM_FIRST <= pointer < RAM_END_EXCLUSIVE:
                    raise ValueError("Original pointer outside installed EMU8000 RAM")
        elif selector == 0x3A and port == 0xA20:
            if pointer is None or not RAM_FIRST <= pointer < RAM_END_EXCLUSIVE:
                raise ValueError("Original source SMLD past installed RAM")
            yielded_words += 1
            yield frame, port, value
            pointer += 1
    if selector == 0x36 and (low is None or high is None):
        raise ValueError("Original RAM trace has trailing partial address")
    if not yielded_words:
        raise ValueError("Original trace has no complete RAM SMLD words")


def write_private_upload_replay(original_trace: Path, private_output: Path):
    """Materialize only copied source port writes, never inside Git.

    The owner must independently establish original Win95/SBAWE.VXD
    provenance and separately verify SF2 PCM identity. This tool
    intentionally cannot authorize the original VXD memory allocator.
    """
    original_trace = original_trace.resolve()
    destination = private_output.resolve()
    repo = Path(__file__).resolve().parents[1]
    if destination == repo or repo in destination.parents:
        raise ValueError("Original RAM upload replay may not enter Git")
    if destination.exists() or destination == original_trace:
        raise ValueError("Cannot overwrite source evidence or existing replay")
    if not original_trace.is_file() or original_trace.stat().st_size > MAX_TRACE_BYTES:
        raise ValueError("Private source capture missing or oversized")
    destination.parent.mkdir(parents=True, exist_ok=True)
    wrote = 0
    try:
        with original_trace.open("rt", encoding="ascii", errors="strict") as src:
            with destination.open("x", encoding="ascii") as out:
                out.write("# P002 original guest source RAM upload PORT WRITES only\n")
                out.write("# SOURCE PROVENANCE REQUIRES INDEPENDENT VERIFICATION\n")
                for frame, port, value in filter_original_ram_upload_port_writes(src):
                    out.write(f"{frame} {port:04X} {value:04X}\n")
                    wrote += 1
        if wrote <= 4:
            raise ValueError("No complete original source RAM upload replay")
    except BaseException:
        destination.unlink(missing_ok=True)
        raise
    return wrote


def sha256_file(path: Path):
    digest = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def analyze(trace: Path, sf2: Path, sample_id: int):
    if not trace.is_file() or not 0 < trace.stat().st_size <= MAX_TRACE_BYTES:
        raise ValueError("missing or oversized PRIVATE original Win95 trace")
    if not sf2.is_file() or sf2.stat().st_size > 32 * 1024 * 1024:
        raise ValueError("missing or oversized PRIVATE SF2 for RAM trace pilot")
    sample = extract_sf2_mono_sample(sf2.read_bytes(), sample_id)
    with trace.open("rt", encoding="ascii", errors="strict") as f:
        ports = scan_capture(f, sample["data"])
    if ports["direct_pcm16_upload_matches"] != 1:
        raise ValueError(
            "original trace must contain this SF2 sample at EXACTLY ONE "
            "physical RAM word address (zero/ambiguous direct matches); "
            "Creative transform/placement NOT PROVEN")
    return {
        "status": "ORIGINAL_CAPTURE_DIRECT_RAM_PCM16_MATCH_CANDIDATE",
        "source_emu8k_trace_sha256": sha256_file(trace),
        "source_sf2_sha256": sha256_file(sf2),
        "original_sf2_sample_id": sample["sample_id"],
        "source_pcm16_sha256": sample["pcm16_sha256"],
        "sample_source_start_word": sample["source_start_word"],
        "sample_source_end_word": sample["source_end_word"],
        "sample_source_loop_start": sample["source_loop_start"],
        "sample_source_loop_end": sample["source_loop_end"],
        "first_matched_captured_emu8000_word_address":
            ports["first_match_emu8000_word_address"],
        "original_chip_trace_evidence": ports,
        "source_guest_win95_SBAWE_VXD_provenance":
            "MUST_BE_VERIFIED_SEPARATELY",
        "original_driver_native_RAM_allocator_reimplemented": False,
        "production_ram_synth_approved": False,
        "actual_VST3_original_driver_PCM_parity": "NOT_PROVEN",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original-emu8k-trace", type=Path, required=True)
    parser.add_argument("--private-sf2", type=Path, required=True)
    parser.add_argument("--sample-id", type=int, required=True)
    parser.add_argument("--private-report", type=Path, required=True)
    parser.add_argument("--private-filtered-ram-replay", type=Path,
                        help="Optional PRIVATE literal original RAM-port writes; never inside Git")
    args = parser.parse_args()
    output = args.private_report.resolve()
    repo = Path(__file__).resolve().parents[1]
    if output == repo or repo in output.parents:
        parser.error("private original trace/SF2 reports MUST stay outside Git")
    if output.exists():
        parser.error("refusing to overwrite existing original evidence")
    report = analyze(args.original_emu8k_trace,args.private_sf2,args.sample_id)
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open("x",encoding="utf-8") as f:
        json.dump(report,f,indent=2)
        f.write("\n")
    if args.private_filtered_ram_replay:
        writes=write_private_upload_replay(
            args.original_emu8k_trace,args.private_filtered_ram_replay)
        print("P002_ORIGINAL_RAM_CAPTURE_LITERAL_PORTS_EXPORTED",
              "exact_original_port_writes="+str(writes),
              "Win95_source_provenance=REQUIRES_SEPARATE_GUEST_AUDIT")
    print("P002_ORIGINAL_RAM_UPLOAD_CAPTURE_SAMPLE_DIRECT_MATCH_CANDIDATE",
          "source_guest_provenance=NOT_INDEPENDENTLY_VERIFIED",
          "actual_RAM_original_driver_implementation=NOT_PROVEN",
          "sample_address_from_original_guest_trace=" +
          str(report["first_matched_captured_emu8000_word_address"]))


if __name__ == "__main__":
    try:
        main()
    except (ValueError,OSError,UnicodeError) as exc:
        print("P002_ORIGINAL_RAM_UPLOAD_TRACE_NO_GO",exc,file=sys.stderr)
        sys.exit(1)
