#!/usr/bin/env python3
"""P002 ORIGINAL Win95 calibrated 86Box capture, owner-side SESSION attestation.

Unlike an invalid fixed guest-LOG SHA256, this verifier accepts an actual
newly BOOTED source session only after reading C:\P002RAM.LOG from the
guest's FAT16 disk, checking its installed ORIGINAL Creative binaries and
corrected CC0=1 test EXE, and matching the genuine complete 260921-word
RAM register upload against that same user SoundFont. The original driver,
VM, ROM, SF2, trace, WAV, log and private manifest stay on the owner's PC.
This is NOT VST3 audio parity and never invents an original chip timestamp.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import sys
import wave

if __package__:
    from .p002_extract_original_win95_ram_log_fat16 import PrivateFat16Reader,sha256_file
    from . import p002_verify_original_creative_ram_transfer as transfer
else:
    from p002_extract_original_win95_ram_log_fat16 import PrivateFat16Reader,sha256_file
    import p002_verify_original_creative_ram_transfer as transfer

SCHEMA="P002_ORIGINAL_WIN95_REAL_BOOT_CAPTURE_V1"
SF2_SHA="d5b99ec38fddf59f53f66b90921e170d207bf3bbd73960f6e4175c39ef68349b"
VXD_SHA="d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a"
DLL_SHA="53b502d0824b75cf8d14c06173ccba5baf53b0c6eff6cd617f771d42ca2d7176"
CORRECTED_GUEST_EXE_SHA="accbf6d6d75d4d4d62bf1326ef1a5365c0b3b9f58b4a32e7b4cd8d7d7a3c9769"
ROM_SHA="4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e"
CALIBRATED_FORK_EXE_SHA="67ec5e58643e32a6dc4efd6e1b42ce2ce7b7da048d218355a54ff39cdc327f16"
EXPECTED_RAM_BASE=0x200010
EXPECTED_SF2_WORDS=260921
EXPECTED_RAM_REWRITES=255
_W=b"WINDOWS    "
_S=b"SYSTEM     "
_A=b"AWE32      "

def _sha_bytes(data:bytes)->str:
    return hashlib.sha256(data).hexdigest()

def _check_wav(path:Path)->dict:
    if not path.is_file() or not 44<=path.stat().st_size<=1024*1024*1024:
        raise ValueError("Source 86Box pre-card WAV is missing/oversized")
    with wave.open(str(path),"rb") as source:
        if (source.getnchannels()!=2 or source.getsampwidth()!=2 or
            source.getframerate()!=44100 or
            source.getcomptype()!="NONE" or
            source.getnframes()<44100):
            raise ValueError("Original pre-card WAV is not 44.1kHz stereo PCM16")
        # A source WAV is NOT proof of note provenance. Do not
        # auto-detect/align C4/D4 from waveform correlation.
        return {"source_pcm_frames":source.getnframes(),
                "sample_rate":44100,"channels":2,"sample_bits":16}

def attest(private_disk:Path, private_original_sf2:Path, original_emu_trace:Path,
           original_wav:Path, original_rom:Path, calibrated_fork_exe:Path
           )->tuple[bytes,dict]:
    paths=[private_disk,private_original_sf2,original_emu_trace,original_wav,
           original_rom,calibrated_fork_exe]
    if len(set(p.resolve() for p in paths))!=len(paths):
        raise ValueError("All six original capture sources must be distinct")
    if not all(p.is_file() for p in paths):
        raise ValueError("Missing actual private Win95 disk, ROM, SF2, trace, WAV or emulator")
    if sha256_file(private_original_sf2)!=SF2_SHA:
        raise ValueError("Owner 83-sample SoundFont original SHA256 mismatch")
    if sha256_file(original_rom)!=ROM_SHA:
        raise ValueError("Private calibrated 86Box original ROM SHA256 mismatch")
    if sha256_file(calibrated_fork_exe)!=CALIBRATED_FORK_EXE_SHA:
        raise ValueError("Not the exact pinned turican0 hardware-calibrated 86Box EXE")
    with PrivateFat16Reader(private_disk) as disk:
        ex=disk.read_root_file(b"P002RAM EXE",1048576)
        bank=disk.read_root_file(b"P002RAM SF2",32*1024*1024)
        vxd=disk.read_short_path_file((_W,_S,b"SBAWE   VXD"),2*1024*1024)
        dll=disk.read_short_path_file((_W,_S,b"AWEMAN32DLL"),2*1024*1024)
        if _sha_bytes(ex)!=CORRECTED_GUEST_EXE_SHA:
            raise ValueError("Guest did not contain the corrected CC0=1 P002RAM.EXE")
        if bank!=private_original_sf2.read_bytes():
            raise ValueError("Guest source SF2 does not match owner's actual original")
        if _sha_bytes(vxd)!=VXD_SHA or _sha_bytes(dll)!=DLL_SHA:
            raise ValueError("Guest Creative 1996 original VXD/DLL hashes invalid")
        # Unlike an unverified fabricated constant, every new valid
        # Win95 guest run has unique GetTickCount values. Extract and
        # validate ACTUAL bytes from the booted disk, never assume a
        # predetermined guest log SHA.
        try:
            log=disk.read_root_file(b"P002RAM LOG",128*1024)
        except ValueError as exc:
            raise ValueError(
                "Windows 95 guest has NOT completed the corrected P002RAM.EXE run: "
                "C:\\P002RAM.LOG absent. No real Creative RAM audio comparison possible"
            ) from exc
    evidence=transfer.analyze(original_emu_trace,private_original_sf2,
                              log,EXPECTED_RAM_BASE)
    if (evidence["converted_source_words_bit_equal"]!=EXPECTED_SF2_WORDS or
        evidence["converted_source_words_mismatched"]!=0 or
        evidence["source_window_overwritten_words"]!=EXPECTED_RAM_REWRITES):
        raise ValueError("Corrected original guest RAM upload does not match actual SoundFont")
    wav_meta=_check_wav(original_wav)
    digest={
        "original_emu8k_trace_sha256":sha256_file(original_emu_trace),
        "original_guest_log_sha256":_sha_bytes(log),
        "original_win95_precard_wav_sha256":sha256_file(original_wav),
        "original_owner_sf2_sha256":sha256_file(private_original_sf2),
        "original_private_booted_disk_sha256":sha256_file(private_disk),
        "original_rom_sha256":sha256_file(original_rom),
        "calibrated_original_fork_exe_sha256":sha256_file(calibrated_fork_exe),
        "corrected_win95_p002ram_exe_sha256":_sha_bytes(ex),
        "installed_original_sbawe_vxd_sha256":_sha_bytes(vxd),
        "installed_original_aweman32_dll_sha256":_sha_bytes(dll),
    }
    result={
        "schema":SCHEMA,
        "original_session_kind":"PRIVATE_CALIBRATED_86BOX_WIN95_CREATIVE_CC0_1",
        **digest,**wav_meta,
        "original_source_sf2_ram_words_bit_equal":EXPECTED_SF2_WORDS,
        "original_source_ram_rewrites":EXPECTED_RAM_REWRITES,
        "guest_log_from_same_private_fat16_disk":True,
        "original_guest_audio_chip_sample_alignment":"MUST_BE_EXTERNALLY_MEASURED",
        "original_trace_and_wav_same_86box_process":
            "MUST_BE_CAPTURED_TOGETHER_BY_START_P002",
        "original_win95_vs_native_vst3_pcm_parity":"NOT_COMPARED",
        "release_approved":False,
    }
    return log,result

def verify_manifest(manifest:Path,trace:Path,sf2:Path,log:Path,wav:Path)->dict:
    if not manifest.is_file() or not 1<=manifest.stat().st_size<=32768:
        raise ValueError("Authenticated original Win95 capture manifest missing")
    try:
        obj=json.loads(manifest.read_text(encoding="utf-8"))
    except (ValueError,UnicodeError) as exc:
        raise ValueError("Invalid original Win95 session manifest") from exc
    if not isinstance(obj,dict) or obj.get("schema")!=SCHEMA:
        raise ValueError("Source Win95 original capture session schema invalid")
    checks={
        "original_emu8k_trace_sha256":sha256_file(trace),
        "original_guest_log_sha256":sha256_file(log),
        "original_win95_precard_wav_sha256":sha256_file(wav),
        "original_owner_sf2_sha256":sha256_file(sf2),
        "corrected_win95_p002ram_exe_sha256":CORRECTED_GUEST_EXE_SHA,
        "installed_original_sbawe_vxd_sha256":VXD_SHA,
        "installed_original_aweman32_dll_sha256":DLL_SHA,
        "original_rom_sha256":ROM_SHA,
        "calibrated_original_fork_exe_sha256":CALIBRATED_FORK_EXE_SHA,
    }
    for key,expected in checks.items():
        if obj.get(key)!=expected:
            raise ValueError("Original Win95 capture session identity mismatch: "+key)
    for key in ("original_private_booted_disk_sha256",):
        v=obj.get(key)
        if not (isinstance(v,str) and len(v)==64 and
                all(c in "0123456789abcdef" for c in v)):
            raise ValueError("Original Win95 booted disk identity missing")
    if (obj.get("original_source_sf2_ram_words_bit_equal")!=EXPECTED_SF2_WORDS or
        obj.get("original_source_ram_rewrites")!=EXPECTED_RAM_REWRITES or
        obj.get("guest_log_from_same_private_fat16_disk") is not True or
        obj.get("original_trace_and_wav_same_86box_process")!=
            "MUST_BE_CAPTURED_TOGETHER_BY_START_P002" or
        obj.get("original_win95_vs_native_vst3_pcm_parity")!="NOT_COMPARED" or
        obj.get("release_approved") is not False):
        raise ValueError("Original source Win95 session evidence incomplete or overclaimed")
    return {
        "status":"ORIGINAL_WIN95_REAL_SESSION_SOURCE_HASHES_VALID",
        "original_emu8k_trace_sha256":checks["original_emu8k_trace_sha256"],
        "original_guest_log_sha256":checks["original_guest_log_sha256"],
        "original_win95_precard_wav_sha256":checks["original_win95_precard_wav_sha256"],
        "same_source_session_claim":"OWNER_SINGLE_86BOX_CAPTURE_REQUIRED",
        "release_approved":False,
    }

def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    for name in ("private-booted-win95-disk","private-original-sf2",
                 "original-emu8k-trace","original-wav",
                 "original-rom","calibrated-86box-exe",
                 "private-extracted-log","private-session-manifest"):
        p.add_argument("--"+name,type=Path,required=True)
    a=p.parse_args()
    sources=[a.private_booted_win95_disk,a.private_original_sf2,
             a.original_emu8k_trace,a.original_wav,a.original_rom,
             a.calibrated_86box_exe]
    outputs=[a.private_extracted_log,a.private_session_manifest]
    source_paths={x.resolve() for x in sources}
    repo=Path(__file__).resolve().parents[1]
    if (len(set(x.resolve() for x in outputs))!=2 or
        any(x.exists() or x.resolve() in source_paths or
            x.resolve()==repo or repo in x.resolve().parents
            for x in outputs)):
        p.error("Private log/manifest must be distinct NEW files outside Git")
    log,result=attest(*sources)
    for path in outputs:
        path.parent.mkdir(parents=True,exist_ok=True)
    try:
        with outputs[0].open("xb") as dst:dst.write(log)
        with outputs[1].open("x",encoding="utf-8") as dst:
            json.dump(result,dst,indent=2,sort_keys=True)
            dst.write("\n")
    except BaseException:
        for path in outputs:path.unlink(missing_ok=True)
        raise
    print("P002_ORIGINAL_WIN95_GUEST_RAM_SOURCE_SESSION_ATTESTED",
          "RAM_words=260921", "real_native_VST3_PCM_parity=NOT_COMPARED",
          "source_manifest_sha256="+sha256_file(outputs[1]))
    return 0

if __name__=="__main__":
    try:sys.exit(main())
    except (ValueError,OSError,wave.Error) as err:
        print("P002_ORIGINAL_WIN95_SESSION_NO_GO",err,file=sys.stderr)
        sys.exit(2)
