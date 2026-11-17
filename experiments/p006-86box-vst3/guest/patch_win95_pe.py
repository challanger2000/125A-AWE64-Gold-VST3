#!/usr/bin/env python3
"""Turn MSVC PE32 /NODEFAULTLIB guest helper into a Win95-targeted PE.

Modern link.exe rejects /SUBSYSTEM:CONSOLE,4.0 (LNK4010) even though
older Win32 accepts a PE optional-header subsystem version of 4.0.
Patch ONLY the PE metadata AFTER linking, validate import DLL names,
and never alter executable code or original Creative/Win95 artifacts.
"""
import argparse
import struct
from pathlib import Path

def u16(b, o): return struct.unpack_from("<H",b,o)[0]
def u32(b, o): return struct.unpack_from("<I",b,o)[0]
def set16(b,o,x): struct.pack_into("<H",b,o,x)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("exe",type=Path)
    a=parser.parse_args()
    data=bytearray(a.exe.read_bytes())
    if len(data)<512 or data[0:2]!=b"MZ":
        raise SystemExit("P006_REJECT_NOT_PE")
    pe=u32(data,0x3c)
    if pe<64 or pe+24+224>len(data) or data[pe:pe+4]!=b"PE\x00\x00":
        raise SystemExit("P006_REJECT_BAD_PE_OFFSET")
    coff=pe+4
    if u16(data,coff)!=0x014c or u16(data,coff+16)<224:
        raise SystemExit("P006_REJECT_NOT_X86")
    opt=pe+24
    if u16(data,opt)!=0x010b or u16(data,opt+68)!=3:
        raise SystemExit("P006_REJECT_NOT_PE32_CONSOLE")
    section_count=u16(data,coff+2)
    section_start=opt+u16(data,coff+16)
    sections=[]
    for i in range(section_count):
        at=section_start+40*i
        if at+40>len(data):raise SystemExit("P006_REJECT_TRUNCATED_SECTIONS")
        size=u32(data,at+16);raw=u32(data,at+20)
        virt=u32(data,at+12);vsize=u32(data,at+8)
        if raw+size>len(data):raise SystemExit("P006_REJECT_TRUNCATED_SECTION")
        sections.append((virt,max(size,vsize),raw,size))
    def rva_offset(rva):
        for virt,total,raw,size in sections:
            if virt<=rva<virt+total and rva-virt<size:
                return raw+rva-virt
        raise SystemExit(f"P006_REJECT_RVA_{rva:08X}")
    # No modern C/C++ runtime: ONLY Kernel32 and WinMM.
    import_rva=u32(data,opt+104)
    import_size=u32(data,opt+108)
    if not import_rva or not import_size:
        raise SystemExit("P006_REJECT_NO_WINMM_IMPORTS")
    imp=rva_offset(import_rva)
    names=set()
    for n in range(32):
        at=imp+20*n
        if at+20>len(data):raise SystemExit("P006_REJECT_IMPORT_OVERFLOW")
        if all(u32(data,at+i)==0 for i in (0,4,8,12,16)):break
        name_off=rva_offset(u32(data,at+12))
        end=data.find(b"\x00",name_off,name_off+100)
        if end<0:raise SystemExit("P006_REJECT_IMPORT_NAME")
        names.add(bytes(data[name_off:end]).decode("ascii").lower())
    else:raise SystemExit("P006_REJECT_IMPORT_COUNT")
    if names!={"kernel32.dll","winmm.dll"}:
        raise SystemExit("P006_REJECT_IMPORTS "+repr(sorted(names)))
    # Win95 (OS 4.0, console subsystem 4.0), disable modern ASLR/NX DLL flags.
    set16(data,opt+40,4);set16(data,opt+42,0)
    set16(data,opt+48,4);set16(data,opt+50,0)
    set16(data,opt+70,0)
    # PE checksum is optional for ordinary user executables.
    struct.pack_into("<I",data,opt+64,0)
    a.exe.write_bytes(data)
    # Recheck what was written, fail even if filesystem content changes.
    changed=a.exe.read_bytes()
    if u16(changed,opt+48)!=4 or u16(changed,opt+50)!=0:
        raise SystemExit("P006_REJECT_METADATA_VERIFY")
    print("P006_WIN95_PE32_NO_CRT_HEADER_PASS: OS=4.0, Subsystem=4.0, "+
          "imports="+",".join(sorted(names)))

if __name__=="__main__":
    main()
