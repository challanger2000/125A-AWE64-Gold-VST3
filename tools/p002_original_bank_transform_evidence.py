#!/usr/bin/env python3
"""P002 research: compare measured ORIGINAL Win95 RAM against one SF2 transform.

Only for independently authenticated private Creative/86Box register traces.
Not a generic Creative bank converter or a VST3/Win95 audio parity gate.
Never upload the owner's SF2/ROM/Windows image, or original private trace.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

try:
    from .p002_original_ram_trace_probe import (
        RAM_FIRST, MAX_RAM_WORDS, MAX_TRACE_BYTES, chunks,
        filter_original_ram_upload_port_writes, sha256_file)
except ImportError:
    from p002_original_ram_trace_probe import (
        RAM_FIRST, MAX_RAM_WORDS, MAX_TRACE_BYTES, chunks,
        filter_original_ram_upload_port_writes, sha256_file)

MAX_SF2 = 32 * 1024 * 1024


def require(ok, message):
    if not ok:
        raise ValueError(message)


def original_bank_pcm16(raw):
    require(12 <= len(raw) <= MAX_SF2 and
            raw[:4] == b"RIFF" and raw[8:12] == b"sfbk" and
            struct.unpack_from("<I", raw, 4)[0] == len(raw)-8,
            "Source SF2 RIFF identity/length invalid")
    roots = list(chunks(raw, 12, len(raw)))

    def section(name):
        hits = [(at, n) for tag, at, n in roots
                if tag == b"LIST" and n >= 4 and raw[at:at+4] == name]
        require(len(hits) == 1, "SF2 requires unique " + repr(name))
        at, size = hits[0]
        return list(chunks(raw, at+4, at+size))

    def one(items, name):
        hits = [(at, n) for tag, at, n in items if tag == name]
        require(len(hits) == 1, "SF2 requires unique " + repr(name))
        return hits[0]

    ifil, n_ifil = one(section(b"INFO"), b"ifil")
    require(n_ifil == 4 and struct.unpack_from("<H", raw, ifil)[0] == 2,
            "Original SF2 is not format 2")
    sdta = section(b"sdta")
    require(not any(tag == b"sm24" for tag, _, _ in sdta),
            "24-bit SF2 source handling unverified")
    start, length = one(sdta, b"smpl")
    hdr, hdr_len = one(section(b"pdta"), b"shdr")
    require(length >= 32 and not length % 2 and hdr_len >= 92 and
            not hdr_len % 46 and hdr_len // 46 <= 65536,
            "Original SF2 smpl/shdr sizes invalid")
    n = length//2
    sample_count = hdr_len//46-1
    source = struct.unpack_from(f"<{n}h", raw, start)
    require(raw[hdr+46*sample_count:hdr+46*sample_count+3] == b"EOS",
            "SF2 missing terminal EOS sample header")
    for i in range(sample_count):
        at = hdr + 46*i
        first, end, loop_first, loop_end, rate = struct.unpack_from(
            "<IIIII", raw, at+20)
        link, kind = struct.unpack_from("<HH", raw, at+42)
        # Actual owner bank includes SEVEN original mono headers whose
        # loop end is precisely sample-end+2, in source guard data.
        # Do not silently change this Creative-readable original SF2.
        require(kind == 1 and link == 0 and rate > 0 and
                0 <= first < end <= n and
                first <= loop_first < loop_end <= min(n, end+2),
                f"Unsupported original SF2 mono sample header {i}")
    return source, sample_count


def observed_three_tap_silent_guard_hypothesis(words):
    """Empirically exact for source SHA d5b99e... ONLY, not all SF2 banks.

    Signed 16-bit original source: (6x[n]-x[n-1]-x[n+1]) >> 3,
    arithmetic negative shift. Runs of >=3 zero words remain zero;
    1- and 2-word zero runs are filtered. Bank word 0 is a guard zero.
    """
    n = len(words)
    require(16 <= n <= MAX_RAM_WORDS and words[0] == 0,
            "Unverified source first-word/size assumptions")
    result = [0]*n
    for i in range(1, n-1):
        result[i] = (6*words[i]-words[i-1]-words[i+1]) >> 3
    result[-1] = (6*words[-1]-words[-2]) >> 3
    run_count = silent_words = 0
    i = 0
    while i < n:
        if words[i]:
            i += 1
            continue
        end = i+1
        while end < n and words[end] == 0:
            end += 1
        if end-i >= 3:
            result[i:end] = [0]*(end-i)
            run_count += 1
            silent_words += end-i
        i = end
    result[0] = 0
    return struct.pack(f"<{n}h", *result), {
        "source_silent_runs_at_least_three": run_count,
        "source_guarded_zero_words": silent_words}


def replay_original_ram(trace):
    require(trace.is_file() and 0 < trace.stat().st_size <= MAX_TRACE_BYTES,
            "Original private EMU8000 register trace missing/oversized")
    ram = bytearray(MAX_RAM_WORDS*2)
    written = bytearray(MAX_RAM_WORDS)
    selector = low = high = pointer = None
    writes = repeated = changed = 0
    # Filter validates original same-frame ordering, RAM port addresses,
    # real SMALW/SMLD pointer halves and out-of-RAM size probes; no writes
    # are synthesized. Preserve historical repeated physical writes.
    with trace.open("rt", encoding="ascii", errors="strict") as events:
        for _, port, value in filter_original_ram_upload_port_writes(events):
            if port == 0xE22:
                selector = value
                if selector == 0x36:
                    low = high = pointer = None
            elif selector == 0x36:
                if port == 0xA20:
                    low = value
                elif port == 0xA22:
                    high = value
                if low is not None and high is not None:
                    pointer = low | (high << 16)
            elif selector == 0x3A and port == 0xA20:
                require(pointer is not None and
                        RAM_FIRST <= pointer < RAM_FIRST+MAX_RAM_WORDS,
                        "Captured original RAM pointer out of installed bounds")
                index = pointer-RAM_FIRST
                previous = struct.unpack_from("<H", ram, index*2)[0]
                if written[index]:
                    repeated += 1
                    changed += int(previous != value)
                written[index] = min(2, written[index]+1)
                struct.pack_into("<H", ram, index*2, value)
                pointer += 1
                writes += 1
    require(writes > 0, "Original trace contains no complete RAM sample writes")
    return ram, written, {
        "captured_original_ram_word_writes": writes,
        "physical_word_rewrite_events": repeated,
        "changed_value_rewrite_events": changed,
        "unique_physical_words_written": sum(bool(x) for x in written)}


def verify(trace, sf2, observed_guard_words):
    require(0 <= observed_guard_words <= 128,
            "Private observed guard offset must be explicit and bounded")
    require(sf2.is_file() and 12 <= sf2.stat().st_size <= MAX_SF2,
            "Private SF2 missing/oversized")
    source, samples = original_bank_pcm16(sf2.read_bytes())
    # The measured coefficient/guard hypothesis is NOT known to apply to
    # other banks. No invented global bank mapping or Creative DSP changes.
    transformed, guard = observed_three_tap_silent_guard_hypothesis(source)
    ram, writes, stats = replay_original_ram(trace)
    start = observed_guard_words*2
    end = start+len(transformed)
    require(end <= len(ram), "Observed sample range outside installed RAM")
    observed = memoryview(ram)[start:end]
    errors = sum(a != b for a, b in zip(observed, transformed))
    region = writes[observed_guard_words:observed_guard_words+len(source)]
    complete = sum(bool(v) for v in region)
    if errors == 0:
        require(complete == len(source),
                "Unwritten physical sample words cannot prove bank parity")
    return {
        "status": ("EXACT_OBSERVED_SINGLE_BANK_TRANSFORM_MATCH"
                   if errors == 0 else "ORIGINAL_BANK_TRANSFORM_MISMATCH"),
        "private_original_trace_sha256": sha256_file(trace),
        "source_sf2_sha256": sha256_file(sf2),
        "source_mono_sample_count": samples,
        "source_pcm16_words": len(source),
        "observed_guard_offset_words": observed_guard_words,
        "sample_window_written_words": complete,
        "sample_window_overwritten_physical_words": sum(v > 1 for v in region),
        "mismatched_final_ram_words": errors,
        "hypothesized_pcm16_sha256": hashlib.sha256(transformed).hexdigest(),
        "observed_final_ram_window_sha256": hashlib.sha256(observed).hexdigest(),
        **stats, **guard,
        "proven_for_other_sf2_banks": False,
        "creative_original_driver_source_provenance": "INDEPENDENT_GUEST_LOG_REQUIRED",
        "native_vst3_ram_sf2_sound_ready": False,
        "original_vs_native_dry_pcm_verified": False,
        "release_approved": False}


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--original-emu8k-trace", type=Path, required=True)
    cli.add_argument("--private-sf2", type=Path, required=True)
    cli.add_argument("--observed-guard-words", type=int, required=True)
    cli.add_argument("--private-report", type=Path, required=True)
    a = cli.parse_args()
    destination = a.private_report.resolve()
    repo = Path(__file__).resolve().parents[1]
    require(not destination.exists() and repo not in destination.parents
            and destination != a.original_emu8k_trace.resolve()
            and destination != a.private_sf2.resolve(),
            "Private report must be NEW, outside Git and source inputs")
    result = verify(a.original_emu8k_trace, a.private_sf2,
                    a.observed_guard_words)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("x", encoding="utf-8") as out:
        json.dump(result, out, indent=2, sort_keys=True)
        out.write("\n")
    print(result["status"], "source_words", result["source_pcm16_words"],
          "mismatches", result["mismatched_final_ram_words"],
          "repeated_physical_addresses",
          result["sample_window_overwritten_physical_words"],
          "native_VST3_RAM_SF2=NOT_PROVEN")
    return 0 if result["mismatched_final_ram_words"] == 0 else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, struct.error) as exc:
        print("P002_PRIVATE_ORIGINAL_BANK_TRANSFORM_NO_GO", exc, file=sys.stderr)
        sys.exit(2)
