#!/usr/bin/env python3
"""P002 strict original Win95 Creative vs independently generated VST3 OUTs.

Compare ONLY bounded EMU8000 16-bit WRITE ports at explicitly supplied
44.1kHz sample-clock origins. Never search for or auto-align a match.
READ parity, original driver state, RAM contents and DRY PCM must be
verified independently. No private trace/source bytes in the repository.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import sys
if __package__:
    from .p002_register_oracle import read_trace
else:
    from p002_register_oracle import read_trace

ORIGINAL_OWNER_RAM_TRACE_SHA256=(
    "4ec6e1764afade0ae965c2ec81cae27b107ff10701c9f727b7d20e06876422f9"
)
HEADER=b"# INDEPENDENT_NATIVE_P002_PORT_WRITES_ONLY\n"
CLOCK_HEADER=b"# CLOCK=HOST_44100_SAMPLE_FRAMES; ORIGIN=ACTIVATION\n"
MAX_WINDOW_FRAMES=44100*60

def sha(path:Path)->str:
    h=hashlib.sha256()
    with path.open("rb") as stream:
        while block:=stream.read(1048576):
            h.update(block)
    return h.hexdigest()

def _has_original_voice_allocator_start(events:list[tuple[int,int,int]])->bool:
    """Recognize one genuine LCOD allocator/stop prologue, not IFATN-only.

    Original VXD source emits (E22,60|voice),(E22,A0|voice),
    (A20,00FF),(E22,60|voice),(620,FFFF), followed by the
    second half of the 32-bit register and the full note plan.
    Presence is only a NECESSARY condition; matching OUTs/PCM still
    cannot authenticate the original driver or clock origin.
    """
    for i in range(len(events)-4):
        _,port,selector=events[i]
        if port!=0xE22 or selector<0x60 or selector>0x7d:
            continue
        voice=selector&0x1f
        expected=((0xE22,0xA0|voice),(0xA20,0x00ff),
                  (0xE22,selector),(0x620,0xffff))
        if all(events[i+j+1][1:] == pair
               for j,pair in enumerate(expected)):
            return True
    return False

def compare_windows(original:Path,native:Path,*,
                    original_origin:int,native_origin:int,
                    frames:int,require_owner_original:bool=True,
                    require_complete_note_start:bool=False,
                    expected_original_sha256:str|None=None)->dict:
    original=original.resolve();native=native.resolve()
    if original==native:
        raise ValueError("Original and native trace must be distinct files")
    if (not 1<=frames<=MAX_WINDOW_FRAMES or
        original_origin<0 or native_origin<0 or
        original_origin>44100*3600 or native_origin>44100*3600):
        raise ValueError("Explicit original/native 44.1kHz frame origins are required")
    with native.open("rb") as f:
        if f.readline()!=HEADER or f.readline()!=CLOCK_HEADER:
            raise ValueError("Missing authentic native VST3 port-write recorder header")
    original_sha=sha(original);native_sha=sha(native)
    expected=expected_original_sha256 or ORIGINAL_OWNER_RAM_TRACE_SHA256
    if (require_owner_original and
        (len(expected)!=64 or
         any(c not in "0123456789abcdef" for c in expected) or
         original_sha!=expected)):
        raise ValueError("Source trace not the independently verified "
                         "original Creative RAM capture/session SHA256")
    def window(path,origin,*,native_write_only=False):
        events=[]
        for frame,direction,port,value in read_trace(path):
            # The real VST3's OUT recorder never emits READ records.
            # Silently filtering fake native READs could accept a
            # forged trace whose WRITE subsequence happens to match.
            if native_write_only and direction!="WRITE":
                raise ValueError("Native VST3 OUT trace must be write-only")
            if direction=="WRITE" and origin<=frame<origin+frames:
                events.append((frame-origin,port,value))
        return events
    a=window(original,original_origin)
    b=window(native,native_origin,native_write_only=True)
    if not a or not b:
        raise ValueError("The requested note window has no source/native writes")
    # A source RAM-upload sequence may be byte-identical on both sides
    # without ever starting a physical voice. This is a NOTE differential,
    # so neither selector-only nor SMALW/SMLD-only traffic qualifies.
    # Observe selection in this same explicit window, then a voice data
    # write; pre-window chip register latches are not inferred.
    for label, events in (("Original",a),("native",b)):
        selected=None
        note_data=False
        for _,port,_value in events:
            if port==0xE22:
                selected=_value
            elif port in (0xE20,0xA20,0xA22,0x620,0x622):
                if selected is not None and selected not in (0x36,0x3A):
                    note_data=True
                    break
        if not note_data:
            raise ValueError(f"{label} note window lacks a selected "
                             "non-RAM EMU8000 voice data write")
    # A single IFATN selector/data pair or arbitrary chip write can
    # match perfectly while starting ZERO physical voices. A COMPLETE
    # note differential, unlike a diagnostic subwindow, requires the
    # original five-write voice allocator/stop prologue on both sides.
    if require_complete_note_start:
        for label,events in (("Original",a),("native",b)):
            if not _has_original_voice_allocator_start(events):
                raise ValueError(
                    f"{label} note window lacks complete original "
                    "Creative physical-voice allocator prologue")
    mismatch=None
    for i in range(max(len(a),len(b))):
        x=a[i] if i<len(a) else None
        y=b[i] if i<len(b) else None
        if x!=y:
            kind=("missing_or_extra_write" if x is None or y is None else
                  next(field for field,v,w in zip(
                      ("relative_frame","port","value"),x,y) if v!=w))
            mismatch={"index":i,"field":kind,"original":x,"native":y}
            break
    return {
        "status":("EQUAL_ORIGINAL_NATIVE_RAM_NOTE_OUT_WINDOW"
                  if mismatch is None else "ORIGINAL_NATIVE_RAM_NOTE_OUT_MISMATCH"),
        "sample_exact_16bit_port_writes":mismatch is None,
        "original_trace_sha256":original_sha,
        "native_trace_sha256":native_sha,
        "owner_original_source_sha_required":require_owner_original,
        "original_session_supplied_sha256_verified":
            require_owner_original and expected_original_sha256 is not None,
        "complete_original_voice_allocator_prologue_required":
            require_complete_note_start,
        "original_origin_frame":original_origin,
        "native_origin_frame":native_origin,
        "window_frames":frames,
        "original_write_count":len(a),
        "native_write_count":len(b),
        "first_mismatch":mismatch,
        "auto_alignment":"NONE",
        "reads_compared":False,
        "source_state_and_midi_identity":"REQUIRES_INDEPENDENT_GUEST_EVIDENCE",
        "original_vs_native_dry_pcm_equal":False,
        "release_approved":False,
    }

def main(argv=None):
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--original",type=Path,required=True)
    cli.add_argument("--native",type=Path,required=True)
    cli.add_argument("--original-origin",type=int,required=True)
    cli.add_argument("--native-origin",type=int,required=True)
    cli.add_argument("--frames",type=int,required=True)
    cli.add_argument("--private-report",type=Path,required=True)
    cli.add_argument("--allow-synthetic-source-only-for-tests",action="store_true")
    args=cli.parse_args(argv)
    output=args.private_report.resolve()
    repo=Path(__file__).resolve().parents[1]
    if (output.exists() or repo==output or repo in output.parents or
        output in (args.original.resolve(),args.native.resolve())):
        cli.error("Private report must be NEW outside source tree and input files")
    result=compare_windows(
        args.original,args.native,original_origin=args.original_origin,
        native_origin=args.native_origin,frames=args.frames,
        require_owner_original=not args.allow_synthetic_source_only_for_tests)
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open("x",encoding="utf-8") as handle:
        json.dump(result,handle,indent=2,sort_keys=True)
        handle.write("\n")
    print(result["status"],"original_writes",result["original_write_count"],
          "native_writes",result["native_write_count"],
          "full_Creative_register_and_DRY_PCM_parity=NOT_PROVEN")
    return 0 if result["sample_exact_16bit_port_writes"] else 1

if __name__=="__main__":
    try:
        sys.exit(main())
    except (OSError,ValueError,UnicodeError) as exc:
        print("P002_ORIGINAL_NATIVE_RAM_PORT_WINDOW_NO_GO",exc,file=sys.stderr)
        sys.exit(2)
