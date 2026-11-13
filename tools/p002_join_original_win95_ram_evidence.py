#!/usr/bin/env python3
"""P002 PRIVATE join of booted Win95, original SF2 and EMU8000 RAM source.

One offline source-only command joins the original Creative guest's
P002RAM.LOG directly from its FAT16 disk, verifies the entire staged
SoundFont matches the host's private SF2 and independently validates
one full, unique 16-bit RAM sample match inside a caller-supplied
original 86Box EMU8K_TRACE. It also optionally describes captured dry
stereo PCM16. No invented original 1996 Creative driver allocator,
hardware PCM parity or fake RAM sample addresses are permitted.

This does NOT launch the private VM. Source provenance of the guest,
Creative DLL/VXD and 86Box recording must be established separately.
Private output files must be new and outside the repository.
"""
from __future__ import annotations

import argparse
from array import array
import hashlib
import json
from pathlib import Path
import sys
import wave

try:
    from .p002_extract_original_win95_ram_log_fat16 import (
        extract_guest_ram_evidence)
    from .p002_original_ram_trace_probe import (
        analyze as analyze_original_ram_trace,
        write_private_upload_replay,
        sha256_file)
except ImportError:
    from p002_extract_original_win95_ram_log_fat16 import (
        extract_guest_ram_evidence)
    from p002_original_ram_trace_probe import (
        analyze as analyze_original_ram_trace,
        write_private_upload_replay,
        sha256_file)


def require(condition: bool, message: str):
    if not condition:
        raise ValueError(message)


def original_dry_pcm_inventory(path: Path):
    """Evidence that source WAV has actual sound; NOT yet native PCM parity."""
    path = path.resolve()
    require(path.is_file() and
            44 <= path.stat().st_size <= 1_000_000_000,
            "Original private 86Box dry WAV is missing/oversized")
    with wave.open(str(path), "rb") as audio:
        if not (audio.getnchannels() == 2 and
                audio.getframerate() == 44100 and
                audio.getsampwidth() == 2 and
                audio.getcomptype() == "NONE" and
                audio.getnframes() >= 44100):
            raise ValueError("Original dry WAV must be 44.1kHz stereo PCM16")
        total=0
        peak=0
        nonzero=0
        clip=0
        for n in iter(lambda: audio.readframes(44100),b""):
            samples=array("h")
            samples.frombytes(n)
            if sys.byteorder!="little":
                samples.byteswap()
            total+=len(samples)
            for v in samples:
                if v!=0:
                    nonzero+=1
                    av=abs(v)
                    peak=max(peak,av)
                    clip+=(av>=32767)
        require(nonzero>=1000 and peak>=64,
                "Captured original 86Box WAV lacks musical-range digital PCM")
        return {
            "private_original_dry_wav_sha256": sha256_file(path),
            "original_86box_pcm16_stereo_frames": total//2,
            "original_86box_pcm16_nonzero_samples": nonzero,
            "original_86box_pcm16_peak": peak,
            "original_86box_pcm16_clipped_samples": clip,
            "original_dry_signal_detected": True,
            "independent_original_driver_VST3_pcm_parity": "NOT_PROVEN",
        }


def analyze_join(private_clone: Path, private_sf2: Path,
                 original_ram_trace: Path, sample_id: int,
                 original_dry_wav: Path | None = None):
    require(sample_id >= 0, "Negative SoundFont sample ID invalid")
    log,guest=extract_guest_ram_evidence(private_clone,private_sf2)
    source=analyze_original_ram_trace(
        original_ram_trace,private_sf2,sample_id)
    require(guest["source_sf2_sha256"]==source["source_sf2_sha256"],
            "Original guest bank differs from source trace bank")
    require(source["original_chip_trace_evidence"][
        "direct_pcm16_upload_matches"]==1,
        "Source sample physical RAM address ambiguous or missing")
    payload={
        "status": "P002_PRIVATE_GUEST_AND_SOURCE_RAM_CAPTURE_JOIN_CANDIDATE",
        "original_driver_API_guest_log": guest,
        "original_emu8000_RAM_source_sample_match": source,
        "private_source_guest_log_sha256": hashlib.sha256(log).hexdigest(),
        "private_source_sf2_sha256": guest["source_sf2_sha256"],
        "private_source_emu8000_trace_sha256":
            source["source_emu8k_trace_sha256"],
        "actual_original_guest_log_fat16_extracted": True,
        "captured_source_sf2_physical_RAM_word_base":
            source["first_matched_captured_emu8000_word_address"],
        "guest_original_win95_driver_vxd_provenance":
            "REQUIRES_INDEPENDENT_VALIDATION",
        "source_bank_load_call_vs_EMU8000_clock_correlation":
            "NOT_PROVEN_FROM_INDEPENDENT_CLOCKS",
        "full_original_SF2_RAM_allocator_verified": False,
        "native_VST3_RAM_SF2_sound_ready": False,
        "original_driver_native_VST3_pcm_parity": "NOT_PROVEN",
        "release_approved": False,
    }
    if original_dry_wav is not None:
        payload["independent_original_dry_PCM_inventory"] = (
            original_dry_pcm_inventory(original_dry_wav))
    else:
        payload["independent_original_dry_PCM_inventory"] = (
            "NO_ORIGINAL_PCM_PROVIDED")
    return log,payload


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--private-booted-win95-clone",type=Path,required=True)
    parser.add_argument("--private-sf2",type=Path,required=True)
    parser.add_argument("--original-emu8k-trace",type=Path,required=True)
    parser.add_argument("--sample-id",type=int,required=True)
    parser.add_argument("--original-dry-wav",type=Path)
    parser.add_argument("--private-log-output",type=Path,required=True)
    parser.add_argument("--private-report",type=Path,required=True)
    parser.add_argument("--private-filtered-ram-ports",type=Path)
    args=parser.parse_args()
    repo=Path(__file__).resolve().parents[1]
    inputs=[p.resolve() for p in (
        args.private_booted_win95_clone,args.private_sf2,
        args.original_emu8k_trace)]
    if args.original_dry_wav:
        inputs.append(args.original_dry_wav.resolve())
    outputs=[args.private_log_output.resolve(),
             args.private_report.resolve()]
    if args.private_filtered_ram_ports:
        outputs.append(args.private_filtered_ram_ports.resolve())
    if len(set(outputs))!=len(outputs) or any(
        p.exists() or p in inputs or p==repo or repo in p.parents
        for p in outputs
    ):
        parser.error("Private output files must be new, unique and outside Git")
    log,report=analyze_join(
        args.private_booted_win95_clone,args.private_sf2,
        args.original_emu8k_trace,args.sample_id,args.original_dry_wav)
    if args.private_filtered_ram_ports:
        # Emit literal original physical upload events, never inferred
        # Creative RAM addresses, but ONLY after whole source/trace
        # and guest identity verification completed.
        write_private_upload_replay(
            args.original_emu8k_trace,args.private_filtered_ram_ports)
    for path in outputs[:2]:
        path.parent.mkdir(parents=True,exist_ok=True)
    with outputs[0].open("xb") as out:
        out.write(log)
    with outputs[1].open("x",encoding="utf-8") as out:
        json.dump(report,out,indent=2)
        out.write("\n")
    print("P002_PRIVATE_ORIGINAL_WIN95_SF2_RAM_CAPTURE_JOIN_CANDIDATE_PASS",
          "guest_FAT16_log_AND_original_SF2_identity=TRUE",
          "exact_source_SAMPLE_RAM_word_address=UNIQUE",
          "original_driver_clock_provenance=NOT_PROVEN",
          "original_VST3_PCM_parity=NOT_PROVEN",
          "release_approved=FALSE")


if __name__ == "__main__":
    try:
        main()
    except (ValueError,OSError,wave.Error) as error:
        print("P002_PRIVATE_ORIGINAL_RAM_CAPTURE_JOIN_NO_GO",
              error,file=sys.stderr)
        sys.exit(1)
