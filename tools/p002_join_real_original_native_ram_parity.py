#!/usr/bin/env python3
"""P002 owner-private REAL Windows-95 Creative RAM vs native VST3 evidence.

This does not replay canned C4/D4 notes or guess MIDI clock alignment.
The original reference is the SHA-pinned owner's actual Win95 guest
log, EMU8K_TRACE, 83-sample SoundFont and 44.1kHz pre-card WAV.
The native side must supply independently recorded VST3 chip-port
OUTs and its pre-CT1745 WAV. All time origins are explicit.
No proprietary inputs/reports are uploaded to CI or stored in GitHub.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import sys
import wave

if __package__:
    from . import p002_verify_original_creative_ram_transfer as transfer
    from . import p002_compare_original_native_ram_note_ports as ports
    from . import p002_compare_original_dry_pcm as pcm
else:
    import p002_verify_original_creative_ram_transfer as transfer
    import p002_compare_original_native_ram_note_ports as ports
    import p002_compare_original_dry_pcm as pcm

ORIGINAL_SF2_SHA256="d5b99ec38fddf59f53f66b90921e170d207bf3bbd73960f6e4175c39ef68349b"
ORIGINAL_TRACE_SHA256=ports.ORIGINAL_OWNER_RAM_TRACE_SHA256
# The exact original guest API log must describe the SAME observed Win95
# driver run as the full source trace and cropped pre-card dry WAV.
# A merely valid/simulated success log from another run is not evidence.
ORIGINAL_GUEST_LOG_SHA256="660a01264550415d50d2cd071e752e9388ebb63071288acbefe95c0386a07096"
ORIGINAL_WAV_SHA256="b4e97086a13cf9c85f20a6e6ec0d9555fb7c708c1d6689fd5f38e0e7c4655dcf"
ORIGINAL_PHYSICAL_RAM_SAMPLE_BASE=0x200010

def _sha(path:Path)->str:
    return ports.sha(path)

def analyze(original_trace:Path, original_sf2:Path, original_guest_log:Path,
            original_wav:Path, native_ports:Path, native_wav:Path,
            *, original_port_origin:int, native_port_origin:int,
            port_frames:int, original_pcm_start:int,
            native_pcm_start:int, pcm_frames:int,
            _synthetic_unit_test_only:bool=False)->dict:
    source_paths=[original_trace,original_sf2,original_guest_log,
                  original_wav,native_ports,native_wav]
    normalized=[p.resolve() for p in source_paths]
    if len(set(normalized))!=len(normalized):
        raise ValueError("Six independent source and native input files required")
    if not all(p.is_file() for p in source_paths):
        raise ValueError("Missing original Win95 or independently recorded native input")
    checks={
        "original_emu8k_trace_sha256":_sha(original_trace),
        "original_owner_sf2_sha256":_sha(original_sf2),
        "original_win95_precard_wav_sha256":_sha(original_wav),
        "native_chip_out_trace_sha256":_sha(native_ports),
        "native_precard_wav_sha256":_sha(native_wav),
        "original_guest_log_sha256":_sha(original_guest_log),
    }
    if not _synthetic_unit_test_only:
        for key,expected in (
            ("original_emu8k_trace_sha256",ORIGINAL_TRACE_SHA256),
            ("original_owner_sf2_sha256",ORIGINAL_SF2_SHA256),
            ("original_win95_precard_wav_sha256",ORIGINAL_WAV_SHA256),
            ("original_guest_log_sha256",ORIGINAL_GUEST_LOG_SHA256)):
            if checks[key]!=expected:
                raise ValueError(
                    "Not independently SHA-pinned ORIGINAL Win95 Creative "
                    "reference: "+key)
    # Authentic original RAM sample upload: include the genuine 255
    # repeated physical sample-word writes. Do NOT use the obsolete
    # first-match/no-overlap candidate in p002_original_ram_trace_probe.
    source=transfer.analyze(
        original_trace,original_sf2,original_guest_log,
        ORIGINAL_PHYSICAL_RAM_SAMPLE_BASE)
    if (source["converted_source_words_mismatched"]!=0 or
        source["converted_source_words_bit_equal"]!=260921 or
        source["source_window_overwritten_words"]!=255):
        raise ValueError("Original source RAM final-state or 255 rewrite provenance invalid")
    # Neither algorithm can infer a clock offset: exact frame coordinates
    # are caller owned, and the original chip upload is not a MIDI clock.
    native=ports.compare_windows(
        original_trace,native_ports,
        original_origin=original_port_origin,
        native_origin=native_port_origin,frames=port_frames,
        require_owner_original=not _synthetic_unit_test_only)
    audio=pcm.compare_pcm16_windows(
        original_wav,native_wav,
        original_pcm_start,native_pcm_start,pcm_frames)
    both=native["sample_exact_16bit_port_writes"] and audio["sample_exact"]
    return {
        "status":("P002_PORT_AND_PRECARD_PCM16_WINDOWS_EXACT"
                  if both else "P002_ORIGINAL_VS_NATIVE_RAM_NOTE_MISMATCH"),
        "source":source,
        "source_file_sha256":checks,
        "native_original_ram_ports":native,
        "native_original_dry_pcm":audio,
        "both_windows_bit_exact":bool(both),
        "source_assets_authentic":not _synthetic_unit_test_only,
        "original_ram_source_rewrites_preserved":255,
        "clock_origin_provenance":"EXTERNAL_MIDI_AND_GUEST_CLOCK_ATTESTATION_REQUIRED",
        "dry_reverb_chorus_zero_sends_proven":"SEPARATE_ORIGINAL_REGISTER_PROOF_REQUIRED",
        "full_general_creative_driver_state_parity":"NOT_PROVEN",
        "real_Win95_vs_exported_VST3_release_approved":False,
    }

def main(argv=None)->int:
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("original-trace","original-sf2","original-guest-log",
                 "original-wav","native-ports","native-wav",
                 "private-report"):
        parser.add_argument("--"+name,required=True,type=Path)
    for name in ("original-port-origin","native-port-origin","port-frames",
                 "original-pcm-start","native-pcm-start","pcm-frames"):
        parser.add_argument("--"+name,required=True,type=int)
    a=parser.parse_args(argv)
    out=a.private_report.resolve()
    root=Path(__file__).resolve().parents[1]
    sources=[a.original_trace,a.original_sf2,a.original_guest_log,
             a.original_wav,a.native_ports,a.native_wav]
    if out.exists() or out in [x.resolve() for x in sources] or (
            out==root or root in out.parents):
        parser.error("Private report must be NEW, outside Git and original/native inputs")
    result=analyze(a.original_trace,a.original_sf2,a.original_guest_log,
                   a.original_wav,a.native_ports,a.native_wav,
                   original_port_origin=a.original_port_origin,
                   native_port_origin=a.native_port_origin,
                   port_frames=a.port_frames,
                   original_pcm_start=a.original_pcm_start,
                   native_pcm_start=a.native_pcm_start,
                   pcm_frames=a.pcm_frames)
    out.parent.mkdir(parents=True,exist_ok=True)
    with out.open("x",encoding="utf-8") as f:
        json.dump(result,f,indent=2,sort_keys=True)
        f.write("\n")
    print(result["status"],
          "original_real_source_RAM=260921_WORDS_BIT_EXACT",
          "native_port_equal="+str(result["native_original_ram_ports"]
                                   ["sample_exact_16bit_port_writes"]).upper(),
          "native_PCM_equal="+str(result["native_original_dry_pcm"]
                                  ["sample_exact"]).upper(),
          "release_approved=FALSE")
    return 0 if result["both_windows_bit_exact"] else 1

if __name__=="__main__":
    try:
        sys.exit(main())
    except (ValueError,OSError,wave.Error) as exc:
        print("P002_ORIGINAL_NATIVE_FINAL_PARITY_NO_GO",exc,file=sys.stderr)
        sys.exit(2)
