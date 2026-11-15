#!/usr/bin/env python3
"""P003: prepare a disposable, already-autobootable P002 Win95 FAT16 clone.

Only operates on the extracted COPY of the existing owner-owned private
86Box VM. Replaces the old C:\\P002RAM.EXE with the newly built P003 guest
program and stages a distinct C:\\P003BANK.SF2. The original private
archive/VM, ROM, Creative driver and original C:\\P002RAM.SF2 are immutable.

This is a FAT16 file update, NOT a new Creative RAM allocator.
"""
from __future__ import annotations
from pathlib import Path
import hashlib
import struct
try:
    from .p002_extract_original_win95_ram_log_fat16 import PrivateFat16Reader
except ImportError:
    from p002_extract_original_win95_ram_log_fat16 import PrivateFat16Reader

FIRST_EXE=b"P002RAM EXE"
FIRST_BANK=b"P002RAM SF2"
SECOND_BANK=b"P003BANKSF2"
GUEST_LOG=b"P002RAM LOG"
FAT_EOC=0xFFFF

def require(ok:bool,message:str):
    if not ok:raise ValueError(message)

def digest(data:bytes)->str:
    return hashlib.sha256(data).hexdigest()

def _entries(root:bytes,short_name:bytes):
    return [(i,root[i:i+32]) for i in range(0,len(root),32)
            if root[i:i+1] not in (b"\x00",b"\xe5") and
            root[i+11]!=0x0f and root[i:i+11]==short_name]

def _chain(fat:bytes,first:int,size:int,cluster_bytes:int,
           cluster_count:int)->list[int]:
    n=(size+cluster_bytes-1)//cluster_bytes
    require(n>0,"P003 cannot replace zero-byte original FAT file")
    out=[];seen=set();at=first
    for i in range(n):
        require(2<=at<cluster_count+2 and at not in seen,
                "P003 original FAT chain corrupt")
        seen.add(at);out.append(at)
        following=struct.unpack_from("<H",fat,at*2)[0]
        if i==n-1:
            require(0xfff8<=following<=0xffff,
                    "P003 original FAT file lacks EOC")
        else:
            require(2<=following<0xfff0 and
                    following<cluster_count+2,
                    "P003 original FAT chain truncated")
            at=following
    return out

def patch_disposable_guest(clone:Path,new_guest_exe:bytes,
                           second_sf2:bytes,
                           expected_original_sf2_sha:str)->dict:
    clone=clone.resolve()
    require(clone.is_file() and len(new_guest_exe)>1024 and
            len(new_guest_exe)<1_000_000 and
            new_guest_exe[:2]==b"MZ",
            "P003 needs compiled Win95 guest PE32 and disposable VM clone")
    require(96<=len(second_sf2)<=32*1024*1024 and
            second_sf2[:4]==b"RIFF" and second_sf2[8:12]==b"sfbk" and
            struct.unpack_from("<I",second_sf2,4)[0]==len(second_sf2)-8,
            "P003 second bank must be valid SoundFont RIFF/sfbk")
    require(len(expected_original_sf2_sha)==64,
            "P003 missing pinned first original SF2 SHA")
    with PrivateFat16Reader(clone) as reader:
        original_sf2=reader.read_root_file(FIRST_BANK,32*1024*1024)
        require(digest(original_sf2)==expected_original_sf2_sha,
                "P003 first original Win95 SoundFont hash mismatch")
        require(digest(second_sf2)!=digest(original_sf2),
                "P003 two different SoundFonts are mandatory")
        original_exe=reader.read_root_file(FIRST_EXE,1_000_000)
        require(original_exe[:2]==b"MZ",
                "P003 old guest executable missing")
        ini=reader.read_short_path_file(
            (b"WINDOWS    ",b"WIN     INI"),32768)
        require(b"run=C:\\P002RAM.EXE" in ini or
                b"run=c:\\p002ram.exe" in ini.lower(),
                "P003 private VM lacks existing P002RAM.EXE automatic startup")
        require(not _entries(reader.root,SECOND_BANK) and
                not _entries(reader.root,GUEST_LOG),
                "P003 refuses existing second SF2 or stale Win95 capture LOG")
        old=_entries(reader.root,FIRST_EXE)
        require(len(old)==1,"P003 original executable root entry not unique")
        old_offset,old_entry=old[0]
        old_size=struct.unpack_from("<I",old_entry,28)[0]
        old_clusters=_chain(reader.fat,
            struct.unpack_from("<H",old_entry,26)[0],
            old_size,reader.cluster_bytes,reader.cluster_count)
        # The second file must use a vacant root directory row.
        vacant=next((i for i in range(0,reader.root_bytes,32)
                     if reader.root[i] in (0,0xe5)),None)
        require(vacant is not None,"P003 FAT16 root has no free row")
        require(reader.root[vacant+11]!=0x0f if reader.root[vacant]==0xe5
                else True,"P003 refuses to overwrite a dangling VFAT row")
        root=bytearray(reader.root)
        fat=bytearray(reader.fat)
        cluster_bytes=reader.cluster_bytes
        needed=[(len(blob)+cluster_bytes-1)//cluster_bytes
                for blob in (new_guest_exe,second_sf2)]
        available=[]
        for cluster in range(2,reader.cluster_count+2):
            if struct.unpack_from("<H",fat,cluster*2)[0]==0:
                available.append(cluster)
                if len(available)==sum(needed):break
        require(len(available)==sum(needed),
                "P003 FAT16 disk lacks free clusters for new two-bank test")
        # Update old EXE only in the throwaway clone, never in original.
        for cluster in old_clusters:
            struct.pack_into("<H",fat,cluster*2,0)
        chunks=[available[:needed[0]],available[needed[0]:]]
        for chain in chunks:
            for index,cluster in enumerate(chain):
                nxt=chain[index+1] if index+1<len(chain) else FAT_EOC
                struct.pack_into("<H",fat,cluster*2,nxt)
        original_entry=bytearray(old_entry)
        struct.pack_into("<H",original_entry,26,chunks[0][0])
        struct.pack_into("<I",original_entry,28,len(new_guest_exe))
        root[old_offset:old_offset+32]=original_entry
        second_entry=bytearray(32)
        second_entry[:11]=SECOND_BANK
        second_entry[11]=0x20
        struct.pack_into("<H",second_entry,26,chunks[1][0])
        struct.pack_into("<I",second_entry,28,len(second_sf2))
        root[vacant:vacant+32]=second_entry
        disk_fat=reader.first_fat
        fat_length=reader.fat_size
        root_at=reader.root_start
        data_at=reader.data_start
    # All preflight checks have completed. Only the disposable copy is
    # mutated; a failed attempt is never booted or interpreted as valid.
    with clone.open("r+b") as disk:
        for blob,chain in zip((new_guest_exe,second_sf2),chunks):
            for i,cluster in enumerate(chain):
                piece=blob[i*cluster_bytes:(i+1)*cluster_bytes]
                disk.seek(data_at+(cluster-2)*cluster_bytes)
                require(disk.write(piece.ljust(cluster_bytes,b"\x00"))==
                        cluster_bytes,"P003 partial FAT data write")
        for copy in (0,1):
            disk.seek(disk_fat+copy*fat_length)
            require(disk.write(fat)==fat_length,
                    "P003 partial FAT mirror write")
        disk.seek(root_at)
        require(disk.write(root)==len(root),
                "P003 partial root write")
        disk.flush()
    with PrivateFat16Reader(clone) as disk:
        require(digest(disk.read_root_file(FIRST_BANK,32*1024*1024))==
                expected_original_sf2_sha,
                "P003 original first bank modified by FAT injection")
        require(disk.read_root_file(FIRST_EXE,1_000_000)==new_guest_exe,
                "P003 Win95 guest executable FAT readback mismatch")
        require(disk.read_root_file(SECOND_BANK,32*1024*1024)==second_sf2,
                "P003 second SF2 FAT readback mismatch")
    return {
        "status":"P003_PRIVATE_DISPOSABLE_TWO_BANK_GUEST_STAGED",
        "guest_exe_sha256":digest(new_guest_exe),
        "original_sf2_sha256":expected_original_sf2_sha,
        "second_sf2_sha256":digest(second_sf2),
        "source_original_archive_modified":False,
        "original_creative_guest_executed":False,
    }
