#!/usr/bin/env python3
"""P008 isolated native AWE32Emu read-only comparative renderer.

Only ORIGINAL private owner-provided ROM/SBK/SF2 inputs can establish the real
Creative/AWE32 sound. The public 'smoke' test explicitly uses synthesized
fallback and does not claim original sound or Studio One/VST3 compatibility.
No private file is uploaded. All WAVs/reports remain in the chosen --out dir.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import os
import struct
import subprocess
import sys
import time
import wave
from pathlib import Path


def vlq(n: int) -> bytes:
    if n < 0:
        raise ValueError("negative tick")
    out = [n & 0x7f]
    n >>= 7
    while n:
        out.insert(0, (n & 0x7f) | 0x80)
        n >>= 7
    return bytes(out)


def make_midi(path: Path, channel: int, program: int,
              note: int, velocity: int, bank: int | None = None,
              hold_ticks: int = 480, tail_ticks: int = 360) -> None:
    if not (0 <= channel <= 15 and 0 <= program <= 127 and
            0 <= note <= 127 and 1 <= velocity <= 127 and
            (bank is None or 0 <= bank <= 127)):
        raise ValueError("MIDI param outside 7-bit range")
    events = bytearray()
    if bank is not None:
        events.extend(b"\0" + bytes((0xb0 | channel, 0, bank)))
        events.extend(b"\0" + bytes((0xb0 | channel, 32, 0)))
    events.extend(b"\0" + bytes((0xc0 | channel, program)))
    events.extend(b"\0" + bytes((0x90 | channel, note, velocity)))
    events.extend(vlq(hold_ticks) + bytes((0x80 | channel, note, 0)))
    events.extend(vlq(tail_ticks) + b"\xff\x2f\x00")
    path.write_bytes(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480)
                     + b"MTrk" + struct.pack(">I", len(events)) + events)


def inspect_wav(path: Path) -> dict:
    with wave.open(str(path), "rb") as w:
        channels, sample_width, sr, frames = (w.getnchannels(), w.getsampwidth(),
                                             w.getframerate(), w.getnframes())
        if channels != 2 or sample_width != 2 or sr not in (44100, 48000):
            raise RuntimeError(f"unexpected WAV {channels}/{sample_width}/{sr}")
        pcm = w.readframes(frames)
    n = len(pcm) // 2
    if n == 0:
        raise RuntimeError("zero PCM frames")
    import array
    a = array.array("h")
    a.frombytes(pcm)
    if sys.byteorder != "little":
        a.byteswap()
    sumsq = sum(int(s)*int(s) for s in a)
    rms = math.sqrt(sumsq/n)/32768
    peak = max(abs(int(s)) for s in a)/32768
    return dict(rate=sr, channels=channels, frames=frames,
                rms=round(rms, 9), peak=round(peak, 9),
                sha256_pcm=hashlib.sha256(pcm).hexdigest())


def sf2_presets(path: Path) -> list[dict]:
    """Read exactly valid RIFF LIST/pdta/phdr; never count terminal EOP."""
    blob = path.read_bytes()
    if len(blob) < 12 or blob[:4] != b"RIFF" or blob[8:12] != b"sfbk":
        raise ValueError(f"bad SF2 header {path}")
    if struct.unpack_from("<I", blob, 4)[0] != len(blob)-8:
        raise ValueError("bad RIFF size")
    chunk = None
    pos = 12
    while pos + 8 <= len(blob):
        tag, length = blob[pos:pos+4], struct.unpack_from("<I", blob, pos+4)[0]
        end = pos + 8 + length
        if end > len(blob):
            raise ValueError("truncated RIFF chunk")
        if tag == b"LIST" and blob[pos+8:pos+12] == b"pdta":
            cursor = pos + 12
            while cursor + 8 <= end:
                sub, width = blob[cursor:cursor+4], struct.unpack_from("<I",blob,cursor+4)[0]
                subend = cursor + 8 + width
                if subend > end:
                    raise ValueError("truncated pdta")
                if sub == b"phdr":
                    chunk = blob[cursor+8:subend]
                    break
                cursor = subend + (width & 1)
        pos = end + (length & 1)
    if not chunk or len(chunk) % 38 or len(chunk) < 76:
        raise ValueError("missing or corrupt phdr")
    presets = []
    for off in range(0, len(chunk)-38, 38):
        name = chunk[off:off+20].split(b"\0")[0].decode("latin1", errors="replace")
        program, bank = struct.unpack_from("<HH", chunk, off+20)
        if program <= 127 and bank <= 128:
            presets.append(dict(name=name, bank=bank, program=program))
    return presets


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=("smoke","private"))
    ap.add_argument("--binary", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--rom", type=Path)
    ap.add_argument("--sbk", type=Path)
    ap.add_argument("--sf2", type=Path, help="owner SF2 with 77 programs")
    ap.add_argument("--other-sf2", type=Path, help="second authentic owner SF2")
    ap.add_argument("--sweep", action="store_true",
                    help="all listed SF2 programs (not just representative note)")
    ap.add_argument("--keep-wav", action="store_true")
    args = ap.parse_args()
    exe = args.binary.resolve()
    if not exe.is_file():
        ap.error(f"native binary missing: {exe}")
    args.out.mkdir(parents=True, exist_ok=True)
    if args.mode == "private":
        for p in (args.rom,args.sbk,args.sf2,args.other_sf2):
            if not p or not p.is_file():
                ap.error("private comparison requires --rom --sbk --sf2 --other-sf2")
    cases = []
    if args.mode == "smoke":
        cases = [("fallback_sine_smoke", 0, 7, 60, 90, None, [], "ours")]
    else:
        rom = [str(args.rom.resolve()), "--sf", str(args.sbk.resolve())]
        sf = ["--rom",str(args.rom.resolve()),"--sf",str(args.sf2.resolve())+"@1"]
        other = ["--rom",str(args.rom.resolve()),"--sf",str(args.other_sf2.resolve())+"@1"]
        cases = [
            ("gm_rom_piano",0,0,60,100,None,
                ["--rom",*rom],"86box"),
            ("gm_rom_melodic7",0,7,60,100,None,
                ["--rom",*rom],"86box"),
            ("gm_rom_kick_ch10",9,0,36,100,None,
                ["--rom",*rom],"86box"),
            ("gm_rom_snare_ch10",9,0,38,100,None,
                ["--rom",*rom],"86box"),
            ("sf2_77_melodic7",0,7,60,100,1,sf,"86box"),
            ("sf2_77_velocity30",0,7,60,30,1,sf,"86box"),
            ("sf2_77_velocity127",0,7,60,127,1,sf,"86box"),
            ("sf2_vintage_melodic7",0,7,60,100,1,other,"86box"),
            ("sf2_vintage_drums_ch10",9,2,36,100,None,other,"86box")
        ]
        if args.sweep:
            presets = sf2_presets(args.sf2)
            seen=set()
            for preset in presets:
                if preset["bank"]!=0:
                    continue
                prog=preset["program"]
                if prog in seen: continue
                seen.add(prog)
                cases.append((f"77_bank_p{prog:03d}",0,prog,60,100,1,sf,"86box"))
            for ch in range(16):
                # Native default Channel10 selects drum bank128 (no user CH10 routing)
                p = 0 if ch==9 else 7
                note=36 if ch==9 else 60
                cases.append((f"midi_ch{ch+1:02d}",ch,p,note,100,None,
                             ["--rom",*rom],"86box"))
    results=[]
    failures=0
    for idx,(name,ch,program,note,vel,bank,assets,chip) in enumerate(cases):
        mid=args.out/(name+".mid")
        wav=args.out/(name+".wav")
        make_midi(mid,ch,program,note,vel,bank)
        cmd=[str(exe),str(mid),"--chip",chip,*assets,"--driver","win95","--wav",str(wav)]
        t0=time.perf_counter()
        proc=subprocess.run(cmd,capture_output=True,text=True,timeout=90)
        elapsed=time.perf_counter()-t0
        row=dict(name=name,channel=ch+1,program=program,
                 pitch=note,velocity=vel,chip=chip,elapsed_seconds=round(elapsed,4),
                 returncode=proc.returncode)
        if proc.returncode==0 and wav.is_file():
            try:
                row.update(inspect_wav(wav))
                row["audible"]=row["rms"]>0.00005 and row["peak"]>0.0001
            except Exception as ex:
                row["error"]=f"WAV decode: {ex}"
        else:
            row["error"]=(proc.stderr+"\n"+proc.stdout)[-1800:]
        row["passed"]=proc.returncode==0 and row.get("audible",False)
        failures+=not row["passed"]
        results.append(row)
        print(f"P008_NATIVE_{'PASS' if row['passed'] else 'FAIL'} "
              f"{idx+1}/{len(cases)} {name} RMS={row.get('rms')} "
              f"peak={row.get('peak')} wall={elapsed:.3f}s",flush=True)
        if not args.keep_wav and wav.exists():
            wav.unlink()
        if mid.exists():
            mid.unlink()
    summary=dict(mode=args.mode,test_count=len(cases),failed_count=failures,
                 upstream_sha="17b0578fe59909dfaa784b5531d6d1a3a80729ce",
                 note="Native AWE32Emu program, NOT a VST3. Private original audio must be verified separately.",
                 source_files={k:str(v.name) for k,v in (
                     ("rom",args.rom),("sbk",args.sbk),
                     ("sf2",args.sf2),("other_sf2",args.other_sf2)) if v},
                 results=results)
    (args.out/"P008-NATIVE-COMPARISON.json").write_text(
        json.dumps(summary,indent=2,ensure_ascii=False),encoding="utf-8")
    (args.out/"P008-NATIVE-COMPARISON.txt").write_text(
        f"Mode: {args.mode}\nUpstream pinned: {summary['upstream_sha']}\n"
        f"Cases: {len(cases)}\nFailed: {failures}\n" +
        "\n".join(f"{x['name']}: {'PASS' if x['passed'] else 'FAIL'} "
                  f"RMS={x.get('rms')} Peak={x.get('peak')} "
                  f"time={x['elapsed_seconds']}" for x in results) + "\n",
        encoding="utf-8")
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
