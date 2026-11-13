#!/usr/bin/env python3
"""P002 synthetic-only fixtures for private original Win95 RAM upload CLI CI.

Public fixture deliberately contains NO original Creative SF2/ROM/VXD
data and NO original Win95 guest capture. Use only to prove that the
independent real-chip offline upload validator behaves fail-closed.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import struct

def chunk(tag: bytes,payload: bytes):
    return tag+struct.pack("<I",len(payload))+payload+bytes(len(payload)&1)

def synthetic_bank(values: tuple[int,...]) -> bytes:
    assert len(values)==4
    pool=[(i*53) & 0xffff for i in range(16)]
    pool[4:8]=values
    smpl=struct.pack("<16H",*pool)
    shdr=(b"SYNTHETIC".ljust(20,b"\0")+
          struct.pack("<5I",4,8,5,7,44100)+
          struct.pack("<BbHH",60,0,0,1))
    eos=b"EOS".ljust(20,b"\0")+bytes(26)
    assert len(shdr)==46 and len(eos)==46
    info=chunk(b"LIST",b"INFO"+chunk(b"ifil",struct.pack("<HH",2,0)))
    sdta=chunk(b"LIST",b"sdta"+chunk(b"smpl",smpl))
    pdta=chunk(b"LIST",b"pdta"+chunk(b"shdr",shdr+eos))
    body=b"sfbk"+info+sdta+pdta
    return b"RIFF"+struct.pack("<I",len(body))+body

def source_trace(words: tuple[int,...],addr=0x200010) -> str:
    lines=[
        "# SYNTHETIC ONLY; never a genuine Win95 driver trace",
        "0 0E22 0036",
        f"0 0A20 {addr & 65535:04X}",
        f"0 0A22 {addr >> 16:04X}",
        "0 0E22 003A",
    ]
    for i,w in enumerate(words):
        lines.append(f"{i} 0A20 {w:04X}")
    return "\n".join(lines)+"\n"

def write(directory: Path):
    directory.mkdir(parents=True,exist_ok=True)
    vals=(0x8001,0x7fff,0x00ff,0xabcd)
    (directory/"synthetic.sf2").write_bytes(synthetic_bank(vals))
    (directory/"wrong-sample.sf2").write_bytes(
        synthetic_bank(vals[:3]+(vals[-1]^1,)))
    (directory/"source-filtered-ram.trace").write_text(
        source_trace(vals),encoding="ascii")
    (directory/"changed-word.trace").write_text(
        source_trace(vals[:3]+(vals[-1]^1,)),encoding="ascii")
    (directory/"wrong-word-address.trace").write_text(
        source_trace(vals,addr=0x200020),encoding="ascii")
    return directory

if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--out",type=Path,required=True)
    args=parser.parse_args()
    write(args.out)
    print("P002_PUBLIC_SYNTHETIC_RAM_SF2_TRACE_FIXTURES_READY no_private_bytes=TRUE")
