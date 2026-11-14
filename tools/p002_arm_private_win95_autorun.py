#!/usr/bin/env python3
"""Prepare a disposable private Win95 86Box clone with P002RAM.EXE autorun.

The owner's original FAT16 disk is READ ONLY. The existing strict staging
tool first clones it and copies P002RAM.EXE / P002RAM.SF2 into the clone.
This then patches ONLY [windows] run= in C:\WINDOWS\WIN.INI of that clone,
preserving the original file's FAT16 cluster and all other configuration.
No Windows 95 system files, Creative drivers, ROM or SoundFonts leave
owner-private storage. This never boots the guest and never proves RAM
upload or PCM output. Stop 86Box fully before reading captured clone.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

try:
    from .p002_stage_original_win95_ram_probe import stage, sha256
    from .p002_extract_original_win95_ram_log_fat16 import (
        PrivateFat16Reader, RAM_EXE_NAME, RAM_SF2_NAME,
        FAT_EOC, FAT_RESERVED_START, MAX_BANK)
except ImportError:
    from p002_stage_original_win95_ram_probe import stage, sha256
    from p002_extract_original_win95_ram_log_fat16 import (
        PrivateFat16Reader, RAM_EXE_NAME, RAM_SF2_NAME,
        FAT_EOC, FAT_RESERVED_START, MAX_BANK)

WINDOWS_DIR=b"WINDOWS    "
WIN_INI=b"WIN     INI"
RUN_VALUE=b"run=C:\\P002RAM.EXE"
MAX_WIN_INI=32768
MAX_DIR_CLUSTERS=32


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def _entry_locations(reader: PrivateFat16Reader, directory: bytes,
                     offsets: list[int], target: bytes, is_dir: bool):
    found=[]
    require(len(directory)==len(offsets)*32 and len(target)==11,
            "FAT16 source directory or 8.3 target invalid")
    for i in range(0,len(directory),32):
        row=directory[i:i+32]
        if row[0]==0:
            break
        if row[0] == 0xE5 or row[11] == 0x0F:
            continue
        if row[:11] == target:
            require(bool(row[11]&0x10) == is_dir and not row[11]&0x08,
                    "Original Win95 FAT16 entry type invalid")
            found.append((offsets[i//32],row))
    require(len(found)==1,
            "Win95 FAT16 directory missing or duplicate: "
            + target.decode("ascii"))
    return found[0]


def _read_dir_chain(reader: PrivateFat16Reader, cluster: int):
    chunks=[]
    offset_positions=[]
    visited=set()
    for _ in range(MAX_DIR_CLUSTERS):
        require(2<=cluster<reader.cluster_count+2 and cluster not in visited,
                "Original Win95 WINDOWS directory FAT16 chain invalid")
        visited.add(cluster)
        disk_offset=reader.data_start+(cluster-2)*reader.cluster_bytes
        data=reader._read(disk_offset,reader.cluster_bytes)
        chunks.append(data)
        offset_positions.extend(disk_offset+i for i in range(
            0,reader.cluster_bytes,32))
        next_cluster=struct.unpack_from("<H",reader.fat,cluster*2)[0]
        if FAT_EOC<=next_cluster<=0xFFFF:
            break
        require(2<=next_cluster<reader.cluster_count+2 and
                next_cluster<FAT_RESERVED_START,
                "Original Win95 WINDOWS FAT16 chain invalid")
        cluster=next_cluster
    else:
        raise ValueError("Original Win95 WINDOWS FAT16 directory too large")
    return b"".join(chunks),offset_positions


def _win_ini_run_replacement(original: bytes) -> bytes:
    require(0<len(original)<=MAX_WIN_INI,
            "Original Win95 WIN.INI missing or oversized")
    require(b"\0" not in original,
            "Original Win95 WIN.INI contains binary/NUL data")
    # Preserve original DOS CRLF and all preexisting system settings.
    require(b"\r\n" in original and
            original.replace(b"\r\n",b"").find(b"\n")==-1,
            "Unknown WIN.INI line endings; no guessed Win95 rewrite")
    lines=original.split(b"\r\n")
    windows_sections=0
    runs=[]
    current=None
    for i,line in enumerate(lines):
        stripped=line.strip()
        if stripped.startswith(b"[") and stripped.endswith(b"]"):
            current=stripped.lower()
            if current==b"[windows]":
                windows_sections+=1
        elif current==b"[windows]":
            if stripped.lower().startswith(b"run="):
                runs.append(i)
            # Require no conflicting existing Win95 LOAD boot command.
            if stripped.lower().startswith(b"load="):
                require(stripped.lower()==b"load=",
                        "Win95 WIN.INI already has an active LOAD startup")
    require(windows_sections==1 and len(runs)==1,
            "Win95 WIN.INI must have exactly one [windows] run= entry")
    run_index=runs[0]
    require(lines[run_index].strip().lower()==b"run=",
            "Win95 WIN.INI RUN startup already occupied; refusing overwrite")
    lines[run_index]=RUN_VALUE
    return b"\r\n".join(lines)


def arm_disposable_clone(clone: Path, source: Path,
                         original_guest_exe: Path,
                         private_sf2: Path) -> dict:
    clone=clone.resolve()
    source=source.resolve()
    original_guest_exe=original_guest_exe.resolve()
    private_sf2=private_sf2.resolve()
    require(clone != source and clone.is_file() and source.is_file(),
            "Only a separately staged Win95 clone may be modified")
    source_sha=sha256(source)
    try:
        with PrivateFat16Reader(clone) as reader:
            # Confirm the strict separate staging tool has copied both
            # inputs to guest C:\ and no old P002RAM.LOG is present.
            require(hashlib.sha256(reader.read_root_file(
                RAM_EXE_NAME,1_000_000)).hexdigest()==sha256(original_guest_exe),
                "Cloned guest P002RAM.EXE differs from input")
            require(hashlib.sha256(reader.read_root_file(
                RAM_SF2_NAME,MAX_BANK)).hexdigest()==sha256(private_sf2),
                "Cloned guest P002RAM.SF2 differs from owner source")
            # Looking up the file intentionally raises only for absence.
            if any(reader.root[i:i+11]==b"P002RAM LOG" and
                   reader.root[i] not in (0,0xE5) and
                   reader.root[i+11]!=0x0F
                   for i in range(0,reader.root_bytes,32)):
                raise ValueError("Clone already has a P002RAM.LOG: refuse overwrite")
            root_offsets=list(reader.root_start+i for i in range(
                0,reader.root_bytes,32))
            _,directory=_entry_locations(
                reader,reader.root,root_offsets,WINDOWS_DIR,True)
            folder=struct.unpack_from("<H",directory,26)[0]
            content,positions=_read_dir_chain(reader,folder)
            ini_entry_offset,ini_entry=_entry_locations(
                reader,content,positions,WIN_INI,False)
            original=reader.read_short_path_file(
                (WINDOWS_DIR,WIN_INI),MAX_WIN_INI)
            current_length=struct.unpack_from("<I",ini_entry,28)[0]
            require(current_length==len(original),
                    "Original Win95 WIN.INI directory length differs")
            first=struct.unpack_from("<H",ini_entry,26)[0]
            require(len(original)<=reader.cluster_bytes and
                    2<=first<reader.cluster_count+2 and
                    struct.unpack_from("<H",reader.fat,first*2)[0]>=FAT_EOC,
                    "Win95 WIN.INI is not an exact single FAT16 cluster")
            updated=_win_ini_run_replacement(original)
            require(len(updated)<=reader.cluster_bytes,
                    "WIN.INI autorun would require additional FAT16 clusters")
            value_offset=reader.data_start+(first-2)*reader.cluster_bytes
            clone_sha_before=sha256(clone)
        # All checks succeeded. Modify ONLY the disposable clone.
        # Writing entire single cluster plus exact directory file length
        # avoids FAT allocation, touching either FAT copy or original OS.
        with clone.open("r+b") as image:
            image.seek(value_offset)
            wrote=image.write(updated.ljust(8192,b"\0"))
            require(wrote==8192,"Unable to patch clone WIN.INI cluster")
            image.seek(ini_entry_offset+28)
            wrote=image.write(struct.pack("<I",len(updated)))
            require(wrote==4,"Unable to patch clone WIN.INI file length")
            image.flush()
        with PrivateFat16Reader(clone) as reader:
            require(reader.read_short_path_file(
                (WINDOWS_DIR,WIN_INI),MAX_WIN_INI)==updated,
                "Win95 WIN.INI autorun readback incorrect")
            require(reader.read_root_file(
                RAM_SF2_NAME,MAX_BANK)==private_sf2.read_bytes(),
                "Original staged guest SoundFont mutated by autorun patch")
            require(hashlib.sha256(reader.read_root_file(
                RAM_EXE_NAME,1_000_000)).hexdigest()==sha256(original_guest_exe),
                "Guest P002RAM.EXE mutated by autorun patch")
        require(sha256(source)==source_sha,
                "Original private Win95 source disk modified unexpectedly")
        return {
            "status":"PRIVATE_WIN95_CLONE_WIN_INI_AUTORUN_ARMED",
            "original_disk_sha256_unchanged":source_sha,
            "clone_sha256_before_patch":clone_sha_before,
            "clone_sha256_after_patch":sha256(clone),
            "modified_file_in_clone":"C:\\WINDOWS\\WIN.INI",
            "guest_autorun":"C:\\P002RAM.EXE",
            "original_win_ini_bytes":len(original),
            "armed_win_ini_bytes":len(updated),
            "guest_creative_driver_executed":False,
            "86Box_guest_booted":False,
            "source_EMU8000_RAM_trace_or_PCM": "NOT_YET_RECORDED",
            "release_approved":False,
        }
    except BaseException:
        # Leave a preexisting clone untouched if validation fails. Only
        # the prepare() wrapper owns/deletes the clone it just created.
        raise


def prepare(source: Path, clone: Path, exe: Path, sf2: Path) -> dict:
    source=source.resolve()
    clone=clone.resolve()
    require(clone!=source and not clone.exists(),
            "New disposable Win95 clone required, never modify original")
    stage_report=stage(source,clone,exe,sf2)
    try:
        run_report=arm_disposable_clone(clone,source,exe,sf2)
        return {**stage_report,**run_report}
    except BaseException:
        # This function owns the newly created clone. On failure it
        # is discarded, never confused with the original VM disk.
        clone.unlink(missing_ok=True)
        raise


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--private-original-win95-disk",type=Path,required=True)
    parser.add_argument("--private-clone",type=Path,required=True)
    parser.add_argument("--win95-p002ram-exe",type=Path,required=True)
    parser.add_argument("--private-sf2",type=Path,required=True)
    parser.add_argument("--private-report",type=Path,required=True)
    args=parser.parse_args()
    destination=args.private_report.resolve()
    repo=Path(__file__).resolve().parents[1]
    if (destination.exists() or destination==repo or repo in destination.parents
            or destination in [x.resolve() for x in (
                args.private_original_win95_disk,args.private_clone,
                args.win95_p002ram_exe,args.private_sf2)]):
        parser.error("Report must be NEW, private, outside Git and inputs")
    result=prepare(args.private_original_win95_disk,
                   args.private_clone,args.win95_p002ram_exe,
                   args.private_sf2)
    destination.parent.mkdir(parents=True,exist_ok=True)
    with destination.open("x",encoding="utf-8") as output:
        json.dump(result,output,indent=2,sort_keys=True)
        output.write("\n")
    print("P002_PRIVATE_WIN95_CLONE_GUEST_AUTORUN_STAGED_PASS",
          "original_disk_unchanged=TRUE",
          "real_Win95_guest_execution=NOT_YET",
          "original_RAM_SF2_PCM=NOT_YET")
    return 0


if __name__=="__main__":
    try:
        sys.exit(main())
    except (OSError,ValueError) as exc:
        print("P002_PRIVATE_WIN95_AUTORUN_STAGING_NO_GO",exc,file=sys.stderr)
        sys.exit(1)
