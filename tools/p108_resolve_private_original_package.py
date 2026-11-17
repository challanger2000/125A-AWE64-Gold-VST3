#!/usr/bin/env python3
"""P108: extract exact private original-VM nested ZIP from known V4 Gesamtpaket.

The two proprietary archives are NEVER uploaded to GitHub; run only in
owner-private sandbox. SHA-pin the original nested owner Win95/86Box package
before extracting, refuse zip slips, duplicates, symlink entries, oversize,
truncation, and mismatching ROM/Creative binaries. Discover exact stress
PE32/SF2 by SHA, not filenames; if not present, fail CLOSED. Never
claim a guest run, x86 trace, P54C clock or PCM parity.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path,PurePosixPath
import shutil
import sys
import zipfile

NESTED_FILENAME="125A-AWE64-P002-Win95-CC0-korrigiert-PRIVATE.zip"
PINNED_ORIGINAL_VM_ARCHIVE_SHA256="ae7b6debdd7024e243f6f4c33f3769c950210e0b68c54abf0eef1f1554d8c1ba"
STRESS_PE_SHA256="3c9306261ed599073f80d9695a8d73140ea65e2de101dc431169c2e6b3f6bc9f"
SECOND_SF2_SHA256="cfeea9897fce687f97160e069f01ae785e7e736798290a0e07ce56af7d8326fb"
MAX_OWNER_ZIP_SIZE=1_200_000_000
MAX_TOTAL_UNPACKED=1_300_000_000
MAX_CANDIDATE_SIZE=32_000_000
REQUIRED_VM=("86Box/86Box.exe","86Box/roms/sound/creative/awe32.raw",
             "86Box/roms/sound/creative/CT4540 PnP.BIN",
             "VM/86box.cfg","VM/win95.img")

def fingerprint(stream):
    d=hashlib.sha256()
    for b in iter(lambda:stream.read(1024*1024),b""):
        d.update(b)
    return d.hexdigest()

def validate_zip(z:zipfile.ZipFile,cap:int):
    files=z.infolist()
    if not files:raise ValueError("original private archive empty")
    seen=set()
    total=0
    for item in files:
        name=item.filename
        parts=PurePosixPath(name).parts
        if (not parts or name.startswith(("/", "\\")) or
            "\\" in name or ":" in name or
            any(p in (".","..") for p in parts) or
            ((item.external_attr>>16)&0o170000)==0o120000):
            raise ValueError("unsafe private ZIP path or symlink")
        if name in seen:raise ValueError("duplicate private ZIP member")
        seen.add(name)
        total+=item.file_size
        if total>cap:raise ValueError("private ZIP inflated size exceeds bound")
    return files

def assert_exact_zip_member_sha(z,name,sha,limit):
    item=z.getinfo(name)
    if item.file_size>limit:raise ValueError("unexpected oversized owner file")
    with z.open(item,"r") as f:
        actual=fingerprint(f)
    if actual!=sha:raise ValueError("original owner SHA-256 mismatch")
    return item

def locate_by_sha(z,sha,max_bytes):
    matches=[]
    for item in z.infolist():
        if item.is_dir() or item.file_size>max_bytes:continue
        with z.open(item,"r") as source:
            if fingerprint(source)==sha:matches.append(item)
    if len(matches)!=1:
        raise ValueError("exact original stress PE32 / SF2 not uniquely in source archive")
    return matches[0]

def extract_member(z,item,dest):
    if dest.exists():raise ValueError("refuse overwrite of private clone source")
    dest.parent.mkdir(parents=True,exist_ok=True)
    with z.open(item,"r") as src, dest.open("xb") as out:
        shutil.copyfileobj(src,out,1024*1024)
    if dest.stat().st_size!=item.file_size:
        raise ValueError("private archive member truncated")

def inspect_original_package(outer:Path,
                            expected_nested_sha:str=PINNED_ORIGINAL_VM_ARCHIVE_SHA256,
                            expected_pe_sha:str=STRESS_PE_SHA256,
                            expected_sf2_sha:str=SECOND_SF2_SHA256):
    if not outer.is_file() or outer.stat().st_size>MAX_OWNER_ZIP_SIZE:
        raise ValueError("private original V4 aggregate missing or oversized")
    with zipfile.ZipFile(outer,"r") as archive:
        members=validate_zip(archive,MAX_TOTAL_UNPACKED)
        nested=[item for item in members
                if PurePosixPath(item.filename).name==NESTED_FILENAME]
        if len(nested)!=1:
            raise ValueError("private V4 package lacks unique pinned original VM archive")
        member=assert_exact_zip_member_sha(archive,nested[0].filename,
                                           expected_nested_sha,MAX_OWNER_ZIP_SIZE)
        pe=locate_by_sha(archive,expected_pe_sha,MAX_CANDIDATE_SIZE)
        sf2=locate_by_sha(archive,expected_sf2_sha,MAX_CANDIDATE_SIZE)
        return member.filename,pe.filename,sf2.filename

def resolve(outer:Path,target:Path,expected_nested_sha=PINNED_ORIGINAL_VM_ARCHIVE_SHA256,
            expected_pe_sha=STRESS_PE_SHA256,expected_sf2_sha=SECOND_SF2_SHA256):
    outer=outer.resolve()
    target=target.resolve()
    if target.exists() or outer==target or target in outer.parents or target in outer.parents:
        raise ValueError("private work path must be a new directory outside source")
    nested,pe,sf2=inspect_original_package(
        outer,expected_nested_sha,expected_pe_sha,expected_sf2_sha)
    target.mkdir(parents=True,exist_ok=False)
    nested_zip=target/"PINNED-ORIGINAL-VM.zip"
    exe=target/"ORIGINAL-STRESS-PE32.exe"
    bank=target/"ORIGINAL-SECOND-BANK.sf2"
    with zipfile.ZipFile(outer,"r") as outerzip:
        extract_member(outerzip,outerzip.getinfo(nested),nested_zip)
        extract_member(outerzip,outerzip.getinfo(pe),exe)
        extract_member(outerzip,outerzip.getinfo(sf2),bank)
    with nested_zip.open("rb") as f:
        if fingerprint(f)!=expected_nested_sha:
            raise ValueError("extracted original VM archive SHA mismatch")
    with exe.open("rb") as f:
        pe_signature=f.read(2)
    if exe.stat().st_size<1024 or pe_signature!=b"MZ":
        raise ValueError("original stress PE32 does not have DOS/PE entry signature")
    with bank.open("rb") as f:
        if f.read(4)!=b"RIFF":
            raise ValueError("original comparison SoundFont not RIFF")
        f.seek(8)
        if f.read(4)!=b"sfbk":
            raise ValueError("original comparison SoundFont not sfbk")
    vm=target/"SOURCE-VM-EXTRACT"
    vm.mkdir()
    with zipfile.ZipFile(nested_zip,"r") as z:
        allfiles=validate_zip(z,MAX_TOTAL_UNPACKED)
        names={i.filename for i in allfiles}
        if not all(x in names for x in REQUIRED_VM):
            raise ValueError("original private source VM missing required files")
        if z.getinfo("VM/win95.img").file_size!=536870912:
            raise ValueError("original FAT16 VM disk size not 512 MiB")
        for item in allfiles:
            if item.is_dir():continue
            extract_member(z,item,vm.joinpath(*PurePosixPath(item.filename).parts))
    # Always validate all 3 extracted fingerprints after archive-copy.
    with exe.open("rb") as f:
        if fingerprint(f)!=expected_pe_sha:raise ValueError("PE32 extraction changed")
    with bank.open("rb") as f:
        if fingerprint(f)!=expected_sf2_sha:raise ValueError("SF2 extraction changed")
    return {"private_original_vm_root":str(vm),
            "private_original_stress_pe32":str(exe),
            "private_original_comparison_sf2":str(bank),
            "source_owner_nested_zip_sha256":expected_nested_sha,
            "original_guest_executed":False,
            "original_emu8000_dsp_modified":False}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--private-original-v4-package",type=Path,required=True)
    p.add_argument("--new-private-work-root",type=Path,required=True)
    args=p.parse_args()
    try:
        r=resolve(args.private_original_v4_package,args.new_private_work_root)
        print("P108_ORIGINAL_V4_PRIVATE_PACKAGE_RESOLVED source_guest_executed=FALSE")
        for k,v in r.items():print(f"{k}={v}")
    except (OSError,ValueError,zipfile.BadZipFile,RuntimeError) as err:
        print("P108_ORIGINAL_PRIVATE_PACKAGE_NO_GO "+str(err),file=sys.stderr)
        sys.exit(2)
if __name__=="__main__":
    main()
