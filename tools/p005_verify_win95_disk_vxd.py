#!/usr/bin/env python3
"""Read-only FAT16 check of installed Creative SBAWE.VXD in a Win95 disk image.

This validates the on-disk driver bytes, NOT the driver loaded in a particular
running guest or the capture time. Never publish the owner's image or VXD.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct

PINNED_SHA256 = "d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a"

class DiskError(ValueError):
    pass

def verify_disk(path: Path) -> dict:
    with path.open("rb") as f:
        mbr=f.read(512)
        if len(mbr)!=512 or mbr[510:512]!=b"\x55\xaa":
            raise DiskError("missing MBR signature")
        partitions=[]
        for i in range(4):
            entry=mbr[446+i*16:462+i*16]
            begin,n=struct.unpack_from("<II",entry,8)
            if entry[4] in (4,6,14) and begin and n:
                partitions.append(begin)
        if len(partitions)!=1:
            raise DiskError("expected exactly one FAT16 partition")
        part=partitions[0]
        f.seek(part*512)
        boot=f.read(512)
        bps=struct.unpack_from("<H",boot,11)[0]
        spc=boot[13]
        reserved=struct.unpack_from("<H",boot,14)[0]
        nfat=boot[16]
        nroot=struct.unpack_from("<H",boot,17)[0]
        fatsz=struct.unpack_from("<H",boot,22)[0]
        if (boot[510:512]!=b"\x55\xaa" or bps!=512 or
                spc==0 or spc&(spc-1) or nfat not in (1,2) or
                not fatsz or not nroot):
            raise DiskError("unsupported FAT16 geometry")
        cbytes=spc*bps
        fatstart=(part+reserved)*bps
        rootstart=fatstart+nfat*fatsz*bps
        datastart=rootstart+((nroot*32+511)//512)*512
        f.seek(fatstart)
        fat=f.read(fatsz*bps)
        f.seek(rootstart)
        root=f.read(nroot*32)
        def read_chain(cluster: int, max_bytes: int=16_000_000)->bytes:
            out=bytearray()
            seen=set()
            while 2<=cluster<0xfff8:
                if cluster in seen or cluster*2+2>len(fat):
                    raise DiskError("invalid FAT16 cluster chain")
                seen.add(cluster)
                if len(out)+cbytes>max_bytes+cbytes:
                    raise DiskError("source exceeds read bound")
                f.seek(datastart+(cluster-2)*cbytes)
                block=f.read(cbytes)
                if len(block)!=cbytes:
                    raise DiskError("short disk image read")
                out.extend(block)
                cluster=struct.unpack_from("<H",fat,cluster*2)[0]
            return bytes(out)
        def find(directory:bytes, name:str, is_dir:bool):
            for off in range(0,len(directory),32):
                e=directory[off:off+32]
                if len(e)<32 or e[0]==0:break
                if e[0]==0xe5 or e[11]==15 or e[11]&8:continue
                base=e[:8].decode("cp437").rstrip()
                ext=e[8:11].decode("cp437").rstrip()
                full=(base+("." + ext if ext else "")).upper()
                if full==name.upper() and bool(e[11]&16)==is_dir:
                    return struct.unpack_from("<H",e,26)[0],struct.unpack_from("<I",e,28)[0]
            raise DiskError("missing source path entry: "+name)
        windows,_=find(root,"WINDOWS",True)
        system,_=find(read_chain(windows),"SYSTEM",True)
        source,size=find(read_chain(system),"SBAWE.VXD",False)
        if not (0<size<=2_000_000):
            raise DiskError("implausible installed VXD size")
        contents=read_chain(source,max_bytes=2_000_000)
        if len(contents)<size:
            raise DiskError("truncated VXD cluster chain")
        digest=hashlib.sha256(contents[:size]).hexdigest()
        if digest!=PINNED_SHA256:
            raise DiskError("installed SBAWE.VXD SHA256 mismatch")
        return {
            "status":"DISK_VXD_SHA256_MATCH_SESSION_PROVENANCE_UNPROVEN",
            "installed_path":r"C:\WINDOWS\SYSTEM\SBAWE.VXD",
            "installed_vxd_bytes":size,
            "installed_vxd_sha256":digest,
            "running_guest_driver_identity_verified":False,
            "capture_bank_load_time_verified":False,
        }

def main()->int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--win95-image",required=True,type=Path)
    args=parser.parse_args()
    try:
        print(json.dumps(verify_disk(args.win95_image),indent=2))
        return 0
    except (OSError,DiskError) as exc:
        print("P005_DISK_ORIGINAL_DRIVER_NO_GO:",exc)
        return 1

if __name__=="__main__":
    raise SystemExit(main())
