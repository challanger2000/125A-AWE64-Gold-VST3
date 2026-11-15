#!/usr/bin/env python3
"""Owner-private P003 original Creative vs earlier exported VST3 differential.

The P003 source is a SHA-pinned actual Win95 capture. The earlier P100
native evidence is NOT a same-session replay: this tool records diagnostics,
never claims final driver/native equivalence. No media leaves the owner system.
No auto time alignment, gain scaling, resampling or silent-window pass.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import sys
import wave
import zipfile

SOURCE_ZIP_SHA = "badf6e05a7d4e06909a5fde706a70f8f4cb6c346f5737176e2a130d2b794ce78"
SOURCE_TRACE_SHA = "181b2bc29a0a3118ea6eb8bc6d921b5447b2c9bf5714356b85d1176e4a6e9548"
ROM_SHA = "4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e"
BANK1_SHA = "d5b99ec38fddf59f53f66b90921e170d207bf3bbd73960f6e4175c39ef68349b"
# Actual note boundaries from the original independent EMU8K_TRACE, not
# waveform matching. Four windows are pinned to the exact session hash above.
ORIGINAL_EVENTS = (("C4_ON", 1632237, 152, 4),
                   ("C4_OFF", 1650400, 8, 0),
                   ("D4_ON", 1679469, 156, 4),
                   ("D4_OFF", 1697634, 8, 0))
FRAMES = 17000  # Before BOTH C4/D4 off events
MAX_ZIP = 200 * 1024 * 1024


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def require(ok, message):
    if not ok:
        raise ValueError(message)


def pcm_samples(raw, start, frames):
    with wave.open(io.BytesIO(raw), "rb") as wav:
        require((wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) ==
                (2, 2, 44100), "Expected authentic stereo PCM16/44100")
        require(start >= 0 and start+frames <= wav.getnframes(),
                "Window outside the actual recorded PCM range")
        wav.setpos(start)
        result = wav.readframes(frames)
    require(len(result) == frames*4, "Truncated PCM")
    return result


def compare_pcm(original, native):
    require(len(original)==len(native) and len(original)>=1024,
            "Empty/unequal PCM windows")
    changed=0
    maxdiff=0
    first=None
    original_audible=0
    for n,(a,b) in enumerate(zip(struct.iter_unpack("<h",original),
                                 struct.iter_unpack("<h",native))):
        diff=abs(a[0]-b[0])
        if a[0]:
            original_audible+=1
        if diff:
            changed+=1
            maxdiff=max(maxdiff,diff)
            if first is None:
                first={"relative_frame":n//2, "channel":n%2}
    require(original_audible>64,"Original audio window is silence")
    return {"compared_channel_samples":len(original)//2,
            "different_channel_samples":changed,
            "first_difference":first,
            "maximum_absolute_difference_pcm16":maxdiff,
            "bit_exact":changed==0,
            "original_window_sha256":digest(original),
            "native_window_sha256":digest(native)}


def writes(raw):
    events=[]
    for line in raw.decode("ascii").splitlines():
        words=line.split()
        if len(words)==3 and words[0].isdigit():
            events.append((int(words[0]),words[1].upper(),int(words[2],16)))
    require(bool(events) and all(a[0]<=b[0] for a,b in zip(events,events[1:])),
            "Invalid/unsorted trace")
    return events


def register_groups(original,native,orig_start,native_start,span,count):
    a=[(t-orig_start,p,v) for t,p,v in original
       if orig_start<=t<=orig_start+span]
    b=[(t-native_start,p,v) for t,p,v in native
       if native_start<=t<=native_start+span]
    require(len(a)==len(b)==count,"Incorrect physical note register count")
    bad=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),None)
    return {"original_writes":len(a),"native_writes":len(b),
            "bit_and_clock_exact":bad is None,
            "first_difference_index":bad,
            "first_original":a[bad] if bad is not None else None,
            "first_native":b[bad] if bad is not None else None}


def member(z,suffix):
    names=[n for n in z.namelist() if n.replace("\\","/").endswith(suffix)]
    require(len(names)==1,"Missing or ambiguous archive member: "+suffix)
    require(0<z.getinfo(names[0]).file_size<64_000_000,
            "Missing/oversized input member")
    return z.read(names[0])


def analyze(source_zip,native_zip,mode):
    require(source_zip.resolve()!=native_zip.resolve(),"Source/native archive alias")
    require(source_zip.stat().st_size<=MAX_ZIP and
            native_zip.stat().st_size<=MAX_ZIP,"Oversized input")
    require(digest(source_zip.read_bytes())==SOURCE_ZIP_SHA,
            "Wrong original Win95 P003 run SHA")
    with zipfile.ZipFile(source_zip) as z:
        m=json.loads(member(z,"P003-BERICHT.json"))
        require(m.get("schema")=="125A-P003-OWNER-TWO-BANK-CAPTURE-V1"
                and m.get("status")=="SOURCE_CREATIVE_TWO_BANK_API_TRACE_WAV_CAPTURED",
                "Unverified original Win95 capture")
        tr=member(z,"Original-Register.trace")
        orig_audio=member(z,"Original-DRY.wav")
        require(digest(tr)==SOURCE_TRACE_SHA,"Wrong original Win95 chip trace")
        for label in ("Original-Register.trace","Original-DRY.wav",
                      "Original-Bank-1.sf2","Vergleichs-Bank-2.sf2",
                      "Creative-Win95-Gast.LOG"):
            require(digest(member(z,label))==m["evidence_sha256"][label],
                    "Original session SHA mismatch: "+label)
        require(m["evidence_sha256"]["Original-Bank-1.sf2"]==BANK1_SHA,
                "Wrong original bank1")
    with zipfile.ZipFile(native_zip) as z:
        meta=json.loads(member(z,mode+".clock.json"))
        require(meta.get("schema")=="P002_NATIVE_REAL_VST3_HOST_CLOCKS_V1"
                and meta.get("original_rom_sha256")==ROM_SHA
                and meta.get("original_owner_sf2_sha256")==BANK1_SHA,
                "Native VST3 source ROM/SF2 provenance mismatch")
        nr=member(z,mode+".out.txt")
        native_audio=member(z,mode+".wav")
        require(digest(nr)==meta["native_chip_out_trace_sha256"] and
                digest(native_audio)==meta["native_precard_wav_sha256"],
                "Native private chip trace/PCM not bound to host manifest")
    src=writes(tr)
    nat=writes(nr)
    native_start=(meta["note_c4_on_frame"],meta["note_c4_off_frame"],
                  meta["note_d4_on_frame"],meta["note_d4_off_frame"])
    groups={}
    for (label,origin,count,span),native_origin in zip(ORIGINAL_EVENTS,native_start):
        groups[label]=register_groups(src,nat,origin,native_origin,span,count)
    pcm={}
    for note,source_origin,native_origin in (("C4",ORIGINAL_EVENTS[0][1],native_start[0]),
                                             ("D4",ORIGINAL_EVENTS[2][1],native_start[2])):
        pcm[note]=compare_pcm(pcm_samples(orig_audio,source_origin,FRAMES),
                              pcm_samples(native_audio,native_origin,FRAMES))
    source_intervals=[ORIGINAL_EVENTS[1][1]-ORIGINAL_EVENTS[0][1],
                      ORIGINAL_EVENTS[2][1]-ORIGINAL_EVENTS[0][1],
                      ORIGINAL_EVENTS[3][1]-ORIGINAL_EVENTS[2][1]]
    native_intervals=[native_start[1]-native_start[0],
                      native_start[2]-native_start[0],
                      native_start[3]-native_start[2]]
    return {"schema":"P003_ORIGINAL_VS_PRIOR_NATIVE_PRIVATE_DIAGNOSTIC_V1",
            "source_zip_sha256":SOURCE_ZIP_SHA,
            "native_zip_sha256":digest(native_zip.read_bytes()),
            "native_mode":mode,
            "comparison_is_matched_same_run":False,
            "second_bank_native_audio_tested":False,
            "source_note_intervals_frames":source_intervals,
            "native_note_intervals_frames":native_intervals,
            "note_intervals_identical":source_intervals==native_intervals,
            "registers":groups,
            "pcm16":pcm,
            "full_original_win95_vs_native_vst3_parity":"NOT_ESTABLISHED",
            "original_emu8000_core_modified":False,
            "release_approved":False}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--original",required=True,type=Path)
    p.add_argument("--native",required=True,type=Path)
    p.add_argument("--native-mode",choices=("independent","source_clocked"),
                   default="independent")
    p.add_argument("--report",required=True,type=Path)
    a=p.parse_args()
    require(not a.report.exists() and a.report.resolve() not in
            (a.original.resolve(),a.native.resolve()),
            "Private report must be NEW and not an input")
    r=analyze(a.original,a.native,a.native_mode)
    a.report.parent.mkdir(parents=True,exist_ok=True)
    a.report.write_text(json.dumps(r,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print("P003_PRIOR_NATIVE_DIAGNOSTIC",
          "C4_PCM_DIFFERENT=",r["pcm16"]["C4"]["different_channel_samples"],
          "D4_PCM_DIFFERENT=",r["pcm16"]["D4"]["different_channel_samples"],
          "full_native_parity=NOT_ESTABLISHED")
    return 0


if __name__=="__main__":
    try:
        sys.exit(main())
    except (OSError,ValueError,KeyError,zipfile.BadZipFile,wave.Error) as e:
        print("P003_PRIVATE_DIAGNOSTIC_NO_GO",e,file=sys.stderr)
        sys.exit(2)
