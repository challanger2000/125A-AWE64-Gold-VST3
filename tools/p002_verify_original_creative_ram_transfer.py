#!/usr/bin/env python3
"""P002 ORIGINAL Creative Win95 SF2->EMU8000 RAM transform evidence audit.

SOURCE-ONLY, RESEARCH-ONLY. The measured three-tap and zero-run-3 behavior
was observed for the owner's Synth 512 Bank (2026-11-13). It is NOT
asserted as the universal historical Creative algorithm, not a VST3
feature, not a ROM or source EMU8000-core modification and NOT proof of
original vs native audio. Never upload private SF2/trace/Win95 to CI.
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
        RAM_FIRST, RAM_END_EXCLUSIVE, MAX_CLOCK_FRAME, MAX_TRACE_LINES,
        MAX_TRACE_BYTES, TRACE, chunks, unique, sha256_file)
    from .p002_verify_original_win95_ram_guest_log import audit
except ImportError:
    from p002_original_ram_trace_probe import (
        RAM_FIRST, RAM_END_EXCLUSIVE, MAX_CLOCK_FRAME, MAX_TRACE_LINES,
        MAX_TRACE_BYTES, TRACE, chunks, unique, sha256_file)
    from p002_verify_original_win95_ram_guest_log import audit

MAX_BANK_BYTES=32*1024*1024


def source_smpl(sf2: bytes) -> tuple[int, ...]:
    if (len(sf2)<96 or sf2[:4]!=b"RIFF" or sf2[8:12]!=b"sfbk" or
            struct.unpack_from("<I",sf2,4)[0]!=len(sf2)-8):
        raise ValueError("Original SF2 RIFF/sfbk identity invalid")
    top=[(tag,start,length) for tag,start,length in chunks(
        sf2,12,len(sf2)) if tag==b"LIST" and length>=4]
    sdta=[(start,length) for tag,start,length in top
          if sf2[start:start+4]==b"sdta"]
    info=[(start,length) for tag,start,length in top
          if sf2[start:start+4]==b"INFO"]
    if len(sdta)!=1 or len(info)!=1:
        raise ValueError("Original SF2 INFO/sdta structure ambiguous")
    info_chunks=list(chunks(sf2,info[0][0]+4,info[0][0]+info[0][1]))
    ifil,n=unique(info_chunks,b"ifil")
    if n!=4 or struct.unpack_from("<H",sf2,ifil)[0]!=2:
        raise ValueError("Original source must be SF2 format 2")
    sdta_chunks=list(chunks(sf2,sdta[0][0]+4,sdta[0][0]+sdta[0][1]))
    if any(tag==b"sm24" for tag,_,_ in sdta_chunks):
        raise ValueError("Original SF2 sm24 not source-validated")
    start,size=unique(sdta_chunks,b"smpl")
    if not size or size%2:
        raise ValueError("Original SF2 smpl length invalid")
    return struct.unpack_from("<"+str(size//2)+"h",sf2,start)


def observed_final_ram_words(lines, base: int, count: int) -> dict:
    """Replay EXACT source original physical port traffic, read-only.

    Source register-bank switches E20/620/622 do not constitute SMLD data.
    Source trace contains out-of-RAM 0xAA55/0x55AA chip-memory-size probes.
    In-range repeated writes retain the FINAL source-observed word and
    are counted, not silently dismissed as first-write-only evidence.
    """
    if not (RAM_FIRST<=base<RAM_END_EXCLUSIVE and
            0<count<=RAM_END_EXCLUSIVE-base):
        raise ValueError("Explicit source sample RAM address/length invalid")
    words=[None]*count
    writes=bytearray(count)
    selector=None
    lo=hi=pointer=None
    prev=-1
    total_events=total_writes=out_of_range_tests=0
    all_ram_writes=0
    for number,raw in enumerate(lines,1):
        if number>MAX_TRACE_LINES:
            raise ValueError("Source register trace exceeds event limit")
        line=raw.strip()
        if not line or line.startswith("#"):
            continue
        match=TRACE.fullmatch(line)
        if match is None:
            raise ValueError(f"Invalid original source trace event {number}")
        read,frame_str,port_str,data_str=match.groups()
        frame,port,value=int(frame_str),int(port_str,16),int(data_str,16)
        if frame<prev or frame>MAX_CLOCK_FRAME:
            raise ValueError("Original source EMU8000 frame order invalid")
        prev=frame
        total_events+=1
        if read:continue
        if port==0xE22:
            if (selector==0x36 and (lo is not None or hi is not None)
                    and (lo is None or hi is None)):
                raise ValueError("Incomplete original RAM SMALW pointer")
            selector=value
            if selector==0x36:
                pointer=None
                lo=hi=None
            continue
        if selector==0x36:
            if port==0xA20:
                if lo is not None:
                    raise ValueError("Duplicate original RAM SMALW low")
                lo=value
            elif port==0xA22:
                if hi is not None:
                    raise ValueError("Duplicate original RAM SMALW high")
                hi=value
            elif port in (0xE20,0x620,0x622):
                continue
            else:
                raise ValueError("Unexpected original RAM SMALW data port")
            if lo is not None and hi is not None:
                pointer=lo|(hi<<16)
        elif selector==0x3A and port==0xA20:
            if pointer is None:
                raise ValueError("Original SMLD without source SMALW pointer")
            if RAM_FIRST<=pointer<RAM_END_EXCLUSIVE:
                all_ram_writes+=1
                if base<=pointer<base+count:
                    idx=pointer-base
                    words[idx]=value
                    if writes[idx]<2:writes[idx]+=1
                    total_writes+=1
            elif value in (0xAA55,0x55AA):
                out_of_range_tests+=1
            else:
                raise ValueError("Unexpected off-RAM SMLD source value")
            pointer+=1
        elif selector==0x3A and port not in (0xE20,0x620,0x622):
            raise ValueError("Unexpected original SMLD data port")
    if (selector==0x36 and (lo is not None or hi is not None)
            and (lo is None or hi is None)):
        raise ValueError("Original trace has dangling SMALW RAM pointer")
    if any(x is None for x in words):
        raise ValueError("Source physical RAM lacks one or more SF2 sample words")
    return {
        "source_ram_words": words,
        "actual_original_register_events": total_events,
        "all_in_range_ram_word_writes":all_ram_writes,
        "observed_source_window_writes":total_writes,
        "source_window_overwritten_words":sum(x==2 for x in writes),
        "original_out_of_range_probe_writes":out_of_range_tests,
    }


def predicted_creative_source_words(source: tuple[int,...]) -> tuple[int,...]:
    """Empirical transform from ONE real 2026-11-13 Creative source bank.

    Only 3+ consecutive zeros retain zero (including 32-word guard
    gaps); one/two zeros are processed via 3 taps. Initial sample word
    is a separate zero. This is not declared a universal Creative codec.
    """
    if len(source)<3 or source[0]!=0:
        raise ValueError("Observed source model requires initial zero word")
    result=[0]*len(source)
    long_zero=bytearray(len(source))
    i=0
    while i<len(source):
        if source[i]!=0:
            i+=1
            continue
        j=i+1
        while j<len(source) and source[j]==0:
            j+=1
        if j-i>=3:
            long_zero[i:j]=b"\x01"*(j-i)
        i=j
    for i in range(1,len(source)-1):
        if long_zero[i]:
            continue
        value=(6*source[i]-source[i-1]-source[i+1])>>3
        if not -32768<=value<=32767:
            raise ValueError("Empirical source transform exceeds signed PCM16")
        result[i]=value & 0xFFFF
    # The final word is the original source's silent trailing guard.
    if source[-1]!=0:
        raise ValueError("Original source SF2 lacks trailing zero guard")
    return tuple(result)


def analyze(trace: Path, sf2_path: Path, guest_log: Path | bytes,
            observed_source_word_base: int) -> dict:
    """Accept either original guest log file or its exact FAT16-extracted bytes.

    Avoid writing proprietary Creative guest evidence to a temporary file
    while joining existing source assets. Both paths use the SAME strict
    original Win95 log auditor and SHA, not a relaxed private shortcut.
    """
    for p,limit in ((trace,MAX_TRACE_BYTES),
                    (sf2_path,MAX_BANK_BYTES)):
        if not p.is_file() or not 1<=p.stat().st_size<=limit:
            raise ValueError("Private original source trace/SF2 missing or oversized")
    if isinstance(guest_log,bytes):
        guest_bytes=guest_log
    else:
        if not guest_log.is_file() or not 1<=guest_log.stat().st_size<=128*1024:
            raise ValueError("Private original Win95 guest log missing or oversized")
        guest_bytes=guest_log.read_bytes()
    if not 1<=len(guest_bytes)<=128*1024:
        raise ValueError("Private original Win95 guest log missing or oversized")
    original_sf2=sf2_path.read_bytes()
    guest=audit(guest_bytes,original_sf2)
    sample=source_smpl(original_sf2)
    # For THIS owner bank the historical source upload has 16 words
    # reserved before the contiguous converted SF2 smpl payload.
    # Never silently choose an address by searching for the best fit.
    prefix=observed_source_word_base-RAM_FIRST
    if prefix<0 or guest["original_driver_ram_allocated_words"]!=len(sample)+prefix:
        raise ValueError("Original guest allocated words do not cover exact source SF2 sample/prefix")
    with trace.open("rt",encoding="ascii",errors="strict") as source:
        replay=observed_final_ram_words(
            source,observed_source_word_base,len(sample))
    pred=predicted_creative_source_words(sample)
    actual=replay.pop("source_ram_words")
    errors=[i for i,(a,b) in enumerate(zip(actual,pred)) if a!=b]
    if errors:
        raise ValueError(
            f"Original source SF2 RAM transform differs in {len(errors)} "
            f"words, first offset {errors[0]}: NO ORIGINAL TRANSFER PARITY")
    return {
        "status":"P002_ACTUAL_ORIGINAL_WIN95_SINGLE_SF2_BANK_RAM_WORD_PARITY",
        "original_guest_log_sha256":hashlib.sha256(guest_bytes).hexdigest(),
        "original_fork_emu8k_trace_sha256":sha256_file(trace),
        "private_original_sf2_sha256":sha256_file(sf2_path),
        "original_physical_ram_sample_base_hex":f"0x{observed_source_word_base:08X}",
        "source_sample_word_count":len(sample),
        "original_creative_reported_ram_allocated_words":
            guest["original_driver_ram_allocated_words"],
        "source_ram_preface_word_count":prefix,
        "converted_source_words_bit_equal":len(sample),
        "converted_source_words_mismatched":0,
        **replay,
        "three_tap_zero_run_ge3":"OBSERVED_ONE_OWNER_BANK_ONLY",
        "original_rom_chip_DSP_modified":False,
        "native_VST3_RAM_SF2_playback_parity":"NOT_VERIFIED",
        "approved_for_production":False,
    }


def main() -> int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--original-emu8k-trace",type=Path,required=True)
    p.add_argument("--original-sf2",type=Path,required=True)
    p.add_argument("--original-win95-guest-log",type=Path,required=True)
    p.add_argument("--observed-original-ram-sample-base",
                   type=lambda x:int(x,0),required=True)
    p.add_argument("--private-report",type=Path,required=True)
    a=p.parse_args()
    target=a.private_report.resolve()
    repository=Path(__file__).resolve().parents[1]
    if (target.exists() or target==repository or repository in target.parents or
        target in (a.original_emu8k_trace.resolve(),
                   a.original_sf2.resolve(),
                   a.original_win95_guest_log.resolve())):
        p.error("Report must be NEW private evidence outside Git/input files")
    evidence=analyze(a.original_emu8k_trace,a.original_sf2,
                     a.original_win95_guest_log,
                     a.observed_original_ram_sample_base)
    target.parent.mkdir(parents=True,exist_ok=True)
    with target.open("x",encoding="utf-8") as out:
        json.dump(evidence,out,indent=2,sort_keys=True)
        out.write("\n")
    print(evidence["status"],
          "exact_source_words=",evidence["converted_source_words_bit_equal"],
          "source_rewrites=",evidence["source_window_overwritten_words"],
          "VST3_PARITY=NOT_VERIFIED",
          "PRODUCTION_APPROVED=FALSE")
    return 0


if __name__=="__main__":
    try:sys.exit(main())
    except (OSError,ValueError) as exc:
        print("P002_ORIGINAL_SOURCE_RAM_TRANSFER_NO_GO",exc,file=sys.stderr)
        sys.exit(2)
