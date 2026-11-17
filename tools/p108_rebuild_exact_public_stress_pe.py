#!/usr/bin/env python3
"""Rebuild SHA-BIT-EXACT public P003 3-bank original Win95 stress PE32.

The historical public artifact from GitHub Actions #43787795937 is *only*
a source-clean 86Box/Win95 test executable, NOT the original proprietary
Creative driver or any owner media.

Public original artifact vs fresh MinGW build was independently compared:
same 23,866 bytes; all PE section SHA hashes equal; ONLY the COFF timestamp
and PE OptionalHeader.CheckSum bytes differ. The original published values
are restored and the WHOLE compiled PE32 MUST match historical original
SHA256 3c930626... If not, fail closed and never stage in private Win95.

The two C source blobs are pinned to the original 2026-11-16 commit.
Does not modify the C source, 86Box, original Creative VxD, ROM or SF2.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

from p108_compare_public_stress_pe import (
    parse_public_pe32,ORIGINAL_PE_SHA256,sha
)

PINNED_C_GIT_BLOB="a3292292bfb6ce6a74de54674e86931ccd8aafff"
PINNED_INCLUDE_GIT_BLOB="d664a2aa3d4d90220a4963f7a1500c50dee11753"
ORIGINAL_PUBLIC_PE32_BYTES=23866
ORIGINAL_COFF_TIMESTAMP=1794793373
ORIGINAL_OPTIONAL_CHECKSUM=46455

def git_blob(raw:bytes):
    raw=raw.replace(b"\r\n",b"\n")
    return hashlib.sha1(
        b"blob "+str(len(raw)).encode()+b"\0"+raw).hexdigest()

def verify_original_public_sources(root:Path):
    src=root/"src"/"win95"/"p002_original_creative_ram_bank_capture.c"
    inc=root/"src"/"win95"/"p003_original_win95_mode_stress.inc"
    if not src.is_file() or not inc.is_file():
        raise ValueError("original public Win95 P003 test C sources missing")
    if (git_blob(src.read_bytes())!=PINNED_C_GIT_BLOB or
        git_blob(inc.read_bytes())!=PINNED_INCLUDE_GIT_BLOB):
        raise ValueError("source original three-bank stress P54C C code changed")
    return src

def restore_public_pe_original_metadata(raw:bytes):
    before=parse_public_pe32(raw)
    if len(raw)!=ORIGINAL_PUBLIC_PE32_BYTES:
        raise ValueError("public original stress PE32 size changed")
    offset=struct.unpack_from("<I",raw,0x3c)[0]
    rebuilt=bytearray(raw)
    # These *two public build metadata values only* came from the
    # SHA-locked original public PE32 artifact, never Creative binaries.
    struct.pack_into("<I",rebuilt,offset+8,ORIGINAL_COFF_TIMESTAMP)
    struct.pack_into("<I",rebuilt,offset+24+64,ORIGINAL_OPTIONAL_CHECKSUM)
    after=parse_public_pe32(rebuilt)
    if before["sections"]!=after["sections"]:
        raise ValueError("public Win95 test code/data sections changed")
    if (after["coff_timestamp"]!=ORIGINAL_COFF_TIMESTAMP or
        after["optional_checksum"]!=ORIGINAL_OPTIONAL_CHECKSUM):
        raise ValueError("failed original public PE32 timestamp/checksum correction")
    return bytes(rebuilt)

def exact_public_guest_from_compiled(source:bytes):
    restored=restore_public_pe_original_metadata(source)
    if sha(restored)!=ORIGINAL_PE_SHA256:
        raise ValueError(
            "compiled public 3-bank PE32 differs beyond authenticated original metadata")
    return restored

def compile_original_public_guest(root:Path):
    src=verify_original_public_sources(root)
    compiler=shutil.which("i686-w64-mingw32-gcc")
    if not compiler:
        raise ValueError("public Win95 i586 MinGW compiler not available")
    with tempfile.TemporaryDirectory(prefix="p108-public-pe32-") as temp:
        exe=Path(temp)/"P002RAM.EXE"
        argv=[
            compiler,"-Os","-Wall","-Wextra","-Werror","-std=c99",
            "-DP003_ORIGINAL_MODE_STRESS=1",
            "-march=pentium","-mtune=pentium","-ffreestanding",
            "-fno-builtin","-fno-stack-protector","-nostdlib",
            "-Wl,-e,_p002_ram_entry@0",
            "-Wl,--disable-dynamicbase,--disable-nxcompat",
            "-Wl,--major-subsystem-version,4",
            "-Wl,--minor-subsystem-version,0",
            "-o",str(exe),str(src),"-lwinmm","-lkernel32","-lgcc"
        ]
        p=subprocess.run(argv,cwd=root,capture_output=True,text=True,timeout=120,
                         check=False)
        if p.returncode or not exe.is_file():
            raise ValueError("public P54C Win95 test compilation failed (not original)")
        rebuilt=exe.read_bytes()
    return exact_public_guest_from_compiled(rebuilt)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--public-repository-root",type=Path,
                    default=Path(__file__).resolve().parents[1])
    ap.add_argument("--new-private-pe32",type=Path,
                    help="Only write after whole-file SHA equals public historical original")
    ap.add_argument("--verify-only",action="store_true")
    args=ap.parse_args()
    if args.verify_only == (args.new_private_pe32 is not None):
        ap.error("use exactly --verify-only or --new-private-pe32")
    try:
        rebuilt=compile_original_public_guest(args.public_repository_root.resolve())
        if args.new_private_pe32:
            target=args.new_private_pe32.resolve()
            target.parent.mkdir(parents=True,exist_ok=True)
            with target.open("xb") as output:output.write(rebuilt)
        print("P108_PUBLIC_WIN95_ORIGINAL_3BANK_STRESS_EXACT_PE32_PASS "
              "whole_sha256="+sha(rebuilt)
              +" source_is_public_only=TRUE private_creative_data=NONE "
              "original_guest_executed=FALSE")
    except (OSError,ValueError,subprocess.SubprocessError) as exc:
        print("P108_ORIGINAL_PUBLIC_STRESS_EXACT_REBUILD_NO_GO "+str(exc),
              file=sys.stderr)
        return 2
    return 0

if __name__=="__main__":
    sys.exit(main())
