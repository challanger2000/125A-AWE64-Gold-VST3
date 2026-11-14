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
    from . import p002_attest_private_win95_session as original_session
else:
    import p002_verify_original_creative_ram_transfer as transfer
    import p002_compare_original_native_ram_note_ports as ports
    import p002_compare_original_dry_pcm as pcm
    import p002_attest_private_win95_session as original_session

ORIGINAL_SF2_SHA256="d5b99ec38fddf59f53f66b90921e170d207bf3bbd73960f6e4175c39ef68349b"
ORIGINAL_TRACE_SHA256=ports.ORIGINAL_OWNER_RAM_TRACE_SHA256
# REAL, independently measured 2026-11-13 original Creative session,
# PR #4 comment #6863236325. Keep it VALID; never confuse its
# fixed SHA with a newly booted corrected-CC0 guest session.
HISTORICAL_GUEST_LOG_SHA256="660a01264550415d50d2cd071e752e9388ebb63071288acbefe95c0386a07096"
HISTORICAL_FULL_138S_WAV_SHA256="eec7dbe6dee6ee08766841fedfa9ed636df067ad94e8fdf03f4e0b8988faebbb"
HISTORICAL_PRIVATE_CROPPED_WAV_SHA256="b4e97086a13cf9c85f20a6e6ec0d9555fb7c708c1d6689fd5f38e0e7c4655dcf"
# A corrected Win95 C:\P002RAM.LOG contains live GetTickCount() data.
# The SHA 660a0126... is VERIFIED for the 2026-11-13 source run
# (PR #4 original 260921-word source trace evidence), but it cannot
# validate a DIFFERENT boot of the newly corrected owner VM.
# Use owner-side FAT16+driver+86Box attested ORIGINAL SESSION manifest:
# source trace/WAV/log SHA are bound to THAT exact execution.
# The previous static source trace/WAV remain historical diagnostic
# references, NOT universal SHA pins for a different VM capture.
ORIGINAL_PHYSICAL_RAM_SAMPLE_BASE=0x200010
ORIGINAL_ROM_SHA256="4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e"
NATIVE_CLOCK_SCHEMA="P002_NATIVE_REAL_VST3_HOST_CLOCKS_V1"

def verify_native_clock_manifest(manifest:Path, native_ports:Path,
                                 native_wav:Path, owner_sf2:Path,
                                 *, note:str,
                                 native_port_origin:int,
                                 native_pcm_start:int)->dict:
    """Owner-side host attestation; does NOT claim Win95 clock origin."""
    if note not in ("C4","D4"):
        raise ValueError("Explicit native C4 or D4 evidence note is required")
    if not manifest.is_file():
        raise ValueError("Native private VST3 host clock manifest missing")
    if manifest.stat().st_size>65536:
        raise ValueError("Native VST3 host manifest exceeds bounded JSON size")
    try:
        meta=json.loads(manifest.read_text(encoding="utf-8"))
    except (UnicodeError,json.JSONDecodeError) as exc:
        raise ValueError("Invalid private VST3 host clock manifest JSON") from exc
    if not isinstance(meta,dict) or meta.get("schema")!=NATIVE_CLOCK_SCHEMA:
        raise ValueError("Unexpected native VST3 host clock schema")
    # JSON booleans are integers in Python, but cannot attest an actual
    # render count, MIDI velocity, or 44.1kHz sample frame.
    def integer(name:str)->int:
        value=meta.get(name)
        if type(value) is not int:
            raise ValueError("Invalid host MIDI clock field: "+name)
        return value
    if (integer("sample_rate")!=44100 or
        integer("host_activation_origin_frame")!=0 or
        integer("midi_channel")!=0 or
        integer("midi_velocity")!=100):
        raise ValueError("Native host sample clock, MIDI channel or velocity mismatch")
    c4=integer("note_c4_on_frame")
    c4_off=integer("note_c4_off_frame")
    d4=integer("note_d4_on_frame")
    d4_off=integer("note_d4_off_frame")
    total=integer("pcm_frames")
    if not 0<=c4<c4_off<d4<d4_off<total<=44100*30:
        raise ValueError("Native C4/D4 MIDI host clock order/length invalid")
    if (integer("audible_c4_on_frames")<64 or
        integer("audible_d4_on_frames")<64):
        raise ValueError("Native host does not attest both audible source notes")
    selected=c4 if note=="C4" else d4
    if native_port_origin!=selected or native_pcm_start!=selected:
        raise ValueError(
            "Native register/PCM window must start at the independently "
            "recorded real host MIDI NoteOn sample clock")
    if (meta.get("original_win95_vs_native_pcm_parity")!="NOT_COMPARED" or
        meta.get("original_creative_driver_provenance")!="NOT_PROVEN_BY_HOST"):
        raise ValueError("Native host cannot declare original Win95 Creative parity")
    for field,actual in (
        ("native_chip_out_trace_sha256",ports.sha(native_ports)),
        ("native_precard_wav_sha256",ports.sha(native_wav)),
        ("original_owner_sf2_sha256",ports.sha(owner_sf2)),
        ("original_rom_sha256",ORIGINAL_ROM_SHA256)):
        if meta.get(field)!=actual:
            raise ValueError("Native VST3 host file SHA256 mismatch: "+field)
    for field in ("exported_vst3_sha256","original_ram_ports_sha256",
                  "original_final_ram_pcm_sha256","original_vxd_pack_sha256"):
        value=meta.get(field)
        if (not isinstance(value,str) or len(value)!=64 or
            any(c not in "0123456789abcdef" for c in value)):
            raise ValueError("Malformed native host source identity: "+field)
    with wave.open(str(native_wav),"rb") as wav:
        if (wav.getnchannels()!=2 or wav.getsampwidth()!=2 or
            wav.getframerate()!=44100 or wav.getnframes()!=total):
            raise ValueError("Native PCM WAV and host timeline contradict")
    return {
        "schema":NATIVE_CLOCK_SCHEMA,
        "host_note":note,
        "host_note_on_frame":selected,
        "native_port_and_pcm_sha256_checked":True,
        "original_sf2_sha256_checked":True,
        "native_midi_clock_host_attested":True,
        "win95_to_host_clock_correlation":"NOT_ESTABLISHED",
        "original_driver_parity":"NOT_PROVEN",
    }

def _sha(path:Path)->str:
    return ports.sha(path)

def analyze(original_trace:Path, original_sf2:Path, original_guest_log:Path,
            original_wav:Path, native_ports:Path, native_wav:Path,
            *, original_port_origin:int, native_port_origin:int,
            port_frames:int, original_pcm_start:int,
            native_pcm_start:int, pcm_frames:int,
            _synthetic_unit_test_only:bool=False,
            native_manifest:Path|None=None,native_note:str="C4",
            original_session_manifest:Path|None=None)->dict:
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
    if not _synthetic_unit_test_only and checks[
            "original_owner_sf2_sha256"]!=ORIGINAL_SF2_SHA256:
        raise ValueError("Not independently SHA-pinned ORIGINAL Win95 Creative "
                         "reference: original_owner_sf2_sha256")
    if original_session_manifest is not None and (
            original_session_manifest.resolve() in normalized):
        raise ValueError("Original session manifest aliases input evidence")
    if _synthetic_unit_test_only:
        source_attestation={
            "status":"SYNTHETIC_SOURCE_ONLY","release_approved":False}
    elif original_session_manifest is not None:
        source_attestation=original_session.verify_manifest(
            original_session_manifest,original_trace,original_sf2,
            original_guest_log,original_wav)
    else:
        # Accept the ACTUAL 2026-11-13 original Creative driver run
        # exactly as attested in PR #4. A freshly booted different VM
        # must instead supply a same-run FAT16+86Box SHA manifest;
        # DO NOT silently extrapolate old hashes to a new log/trace.
        if (checks["original_emu8k_trace_sha256"]!=ORIGINAL_TRACE_SHA256 or
            checks["original_guest_log_sha256"]!=HISTORICAL_GUEST_LOG_SHA256 or
            checks["original_win95_precard_wav_sha256"] not in (
                HISTORICAL_FULL_138S_WAV_SHA256,
                HISTORICAL_PRIVATE_CROPPED_WAV_SHA256)):
            raise ValueError(
                "Original Win95 source not the verified 2026-11-13 "
                "historic capture: a NEW boot requires its separately "
                "authenticated original-session manifest")
        source_attestation={
            "status":"AUTHENTIC_ORIGINAL_WIN95_20261113_SHA_TRIPLE",
            "original_emu8k_trace_sha256":ORIGINAL_TRACE_SHA256,
            "original_guest_log_sha256":HISTORICAL_GUEST_LOG_SHA256,
            "original_win95_precard_wav_sha256":
                checks["original_win95_precard_wav_sha256"],
            "historical_provenance":"PR4_6863236325",
            "release_approved":False}

    if not _synthetic_unit_test_only and native_manifest is None:
        raise ValueError("Production parity requires authentic private native "
                         "host SHA256 and MIDI clock manifest")
    if native_manifest is not None and native_manifest.resolve() in normalized:
        raise ValueError("Native host manifest aliases original or native input")
    if (native_manifest is not None and original_session_manifest is not None
            and native_manifest.resolve()==original_session_manifest.resolve()):
        raise ValueError("Source Win95 and native VST3 manifests must be independent")
    native_clock=(verify_native_clock_manifest(
        native_manifest,native_ports,native_wav,original_sf2,
        note=native_note,native_port_origin=native_port_origin,
        native_pcm_start=native_pcm_start)
        if native_manifest is not None else {
            "native_midi_clock_host_attested":False,
            "original_driver_parity":"NOT_PROVEN_TEST_ONLY"})
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
        require_owner_original=not _synthetic_unit_test_only,
        require_complete_note_start=not _synthetic_unit_test_only,
        expected_original_sha256=(
            source_attestation["original_emu8k_trace_sha256"]
            if not _synthetic_unit_test_only else None))
    audio=pcm.compare_pcm16_windows(
        original_wav,native_wav,
        original_pcm_start,native_pcm_start,pcm_frames)
    both=native["sample_exact_16bit_port_writes"] and audio["sample_exact"]
    return {
        "status":("P002_PORT_AND_PRECARD_PCM16_WINDOWS_EXACT"
                  if both else "P002_ORIGINAL_VS_NATIVE_RAM_NOTE_MISMATCH"),
        "source":source,
        "source_file_sha256":checks,
        "original_win95_source_session":source_attestation,
        "native_host_clock_and_sha256":native_clock,
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
    parser.add_argument("--original-session-manifest",type=Path,
                        help="Required for NEW Win95 captures; optional only for "
                             "the exact independently verified 2026-11-13 source")
    parser.add_argument("--native-manifest",required=True,type=Path)
    parser.add_argument("--native-note",required=True,choices=("C4","D4"))
    a=parser.parse_args(argv)
    out=a.private_report.resolve()
    root=Path(__file__).resolve().parents[1]
    sources=[a.original_trace,a.original_sf2,a.original_guest_log,
             a.original_wav,a.native_ports,a.native_wav,a.native_manifest]
    if a.original_session_manifest is not None:
        sources.append(a.original_session_manifest)
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
                   pcm_frames=a.pcm_frames,
                   native_manifest=a.native_manifest,
                   native_note=a.native_note,
                   original_session_manifest=a.original_session_manifest)
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
