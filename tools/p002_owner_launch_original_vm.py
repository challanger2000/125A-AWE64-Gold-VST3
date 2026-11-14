#!/usr/bin/env python3
"""ONE Windows-owner launch: boot the EXISTING corrected private Win95
Creative guest with the PINNED calibrated 86Box fork, and locally attest
its actual live source RAM register/WAV/session (not an invented trace).

Never uploads proprietary VM, Win95, ROM, VXD, SoundFont or capture.
The VM EXE has a normal interactive GUI; the owner exits it after test.
Python is the only host dependency; no DAW/ASIO is involved.
"""
from __future__ import annotations
import argparse
from datetime import datetime,timezone
import hashlib
import os
from pathlib import Path,PurePosixPath
import secrets
import subprocess
import sys
import zipfile

if __package__:
    from .p002_attest_private_win95_session import (
        attest,sha256_file,ROM_SHA,CALIBRATED_FORK_EXE_SHA,SF2_SHA,
        CORRECTED_GUEST_EXE_SHA)
    from .p002_extract_original_win95_ram_log_fat16 import PrivateFat16Reader
else:
    from p002_attest_private_win95_session import (
        attest,sha256_file,ROM_SHA,CALIBRATED_FORK_EXE_SHA,SF2_SHA,
        CORRECTED_GUEST_EXE_SHA)
    from p002_extract_original_win95_ram_log_fat16 import PrivateFat16Reader
import json

PRIVATE_ARCHIVE_REQUIRED=(
    "86Box/86Box.exe",
    "86Box/roms/sound/creative/awe32.raw",
    "86Box/roms/sound/creative/CT4540 PnP.BIN",
    "86Box/roms/machines/p55t2p4/0207_j2.bin",
    "VM/win95.img","VM/86box.cfg",
)
MAX_EXTRACTED_BYTES=1_100_000_000

def verified_zip(path:Path)->dict:
    if not path.is_file():
        raise ValueError("Private corrected Win95 zip not found")
    with zipfile.ZipFile(path) as source:
        entries=source.infolist()
        names={i.filename for i in entries}
        if len(names)!=len(entries) or not all(x in names for x in PRIVATE_ARCHIVE_REQUIRED):
            raise ValueError("Incomplete/duplicated private calibrated Win95 package")
        total=0
        for item in entries:
            parts=PurePosixPath(item.filename).parts
            if (not parts or item.filename.startswith(("/","\\")) or
                any(x in ("",".","..") for x in parts) or
                ":" in item.filename or "\\" in item.filename):
                raise ValueError("Refuse private ZIP path traversal or absolute drive")
            total+=item.file_size
            if total>MAX_EXTRACTED_BYTES:
                raise ValueError("Refuse private ZIP oversized expanded content")
        if source.getinfo("VM/win95.img").file_size!=536_870_912:
            raise ValueError("Not the real original Win95 512MiB FAT16 clone")
        def hash_member(name):
            digest=hashlib.sha256()
            with source.open(name) as stream:
                for block in iter(lambda:stream.read(1024*1024),b""):
                    digest.update(block)
            return digest.hexdigest()
        if hash_member("86Box/86Box.exe")!=CALIBRATED_FORK_EXE_SHA:
            raise ValueError("Incorrect/non-hardware-calibrated 86Box.exe")
        if hash_member("86Box/roms/sound/creative/awe32.raw")!=ROM_SHA:
            raise ValueError("Incorrect source AWE32 ROM")
        return {"private_zip_sha256":sha256_file(path),
                "hardware_calibrated_86box_sha256":CALIBRATED_FORK_EXE_SHA,
                "original_rom_sha256":ROM_SHA,
                "private_win95_image_bytes":536_870_912,
                "validated_archive_members":len(entries),
                "booted_or_creative_audio_tested":False}

def run(archive:Path, work_root:Path)->dict:
    if os.name!="nt":
        raise ValueError("Actual Windows95/86Box run REQUIRES Windows 11 x64 "
                         "and this private executable cannot run as a Linux test")
    if work_root.exists():
        raise ValueError("Refusing reused Win95 session directory")
    verified_zip(archive)
    work_root.mkdir(parents=True,exist_ok=False)
    with zipfile.ZipFile(archive) as source:
        source.extractall(work_root)
    disk=work_root/"VM/win95.img"
    rom=work_root/"86Box/roms/sound/creative/awe32.raw"
    emulator=work_root/"86Box/86Box.exe"
    if sha256_file(rom)!=ROM_SHA or sha256_file(emulator)!=CALIBRATED_FORK_EXE_SHA:
        raise ValueError("Original hardware-calibrated emulator mutated after unzip")
    # NO stale log; fresh clone MUST create new one in this actual run.
    with PrivateFat16Reader(disk) as guest:
        if hashlib.sha256(guest.read_root_file(
                b"P002RAM EXE",1048576)).hexdigest()!=CORRECTED_GUEST_EXE_SHA:
            raise ValueError("Corrected private Win95 CC0=1 program not staged")
        sf2=guest.read_root_file(b"P002RAM SF2",32*1024*1024)
        if hashlib.sha256(sf2).hexdigest()!=SF2_SHA:
            raise ValueError("Original guest SF2 hash wrong before boot")
        try:
            guest.read_root_file(b"P002RAM LOG",128*1024)
        except ValueError:
            pass
        else:
            raise ValueError("Win95 image contains an OLD P002RAM.LOG: "
                             "fresh original guest required")
    capture=work_root/"P002-Nachweise"
    capture.mkdir(exist_ok=True)
    stamp=datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    token=secrets.token_hex(6)
    prefix=f"P002-ORIGINAL-{stamp}-{token}"
    trace=capture/(prefix+".trace")
    wav=capture/(prefix+".wav")
    log=capture/(prefix+"-P002RAM.LOG")
    bank=capture/(prefix+"-source.sf2")
    manifest=capture/(prefix+"-original-session.json")
    with bank.open("xb") as out:
        out.write(sf2)
    # Both filenames have one common, unique run tag and were verified
    # nonexistent before launching this precise calibrated executable.
    env=os.environ.copy()
    env.update({"EMU8K_TRACE":str(trace),"AWE32_WAV":str(wav)})
    # Only the original private turican0 86Box launches. The original
    # Creative guest auto-starts P002RAM.EXE via its existing WIN.INI.
    # No source driver mutation and no guest data uploaded.
    result=subprocess.run([str(emulator),"-P",str(work_root/"VM"),
                           "-R",str(work_root/"86Box/roms")],
                          cwd=emulator.parent,env=env,check=False)
    if result.returncode!=0:
        raise ValueError("Original calibrated Win95 86Box failed; NO parity pass")
    if not trace.is_file() or not wav.is_file():
        raise ValueError("Original 86Box quit without BOTH independently "
                         "recorded register and PCM capture files")
    guest_log,evidence=attest(disk,bank,trace,wav,rom,emulator)
    with log.open("xb") as out:
        out.write(guest_log)
    try:
        with manifest.open("x",encoding="utf-8") as out:
            json.dump(evidence,out,indent=2,sort_keys=True)
            out.write("\n")
    except BaseException:
        log.unlink(missing_ok=True)
        raise
    print("P002_REAL_WIN95_CREATIVE_ORIGINAL_CAPTURE_VERIFIED",
          "private_trace="+str(trace),"private_wav="+str(wav),
          "private_guest_log="+str(log),
          "original_session_manifest="+str(manifest),
          "native_VST3_parity=NOT_COMPARED")
    return evidence

def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--private-original-archive",type=Path,required=True)
    p.add_argument("--new-private-workdir",type=Path)
    p.add_argument("--archive-preflight-only",action="store_true")
    a=p.parse_args()
    report=verified_zip(a.private_original_archive)
    if a.archive_preflight_only:
        print(json.dumps(report,indent=2,sort_keys=True))
        print("P002_OWNER_ARCHIVE_VERIFIED_ONLY: WIN95_NOT_BOOTED; "
              "ORIGINAL_CREATIVE_AUDIO_NOT_RECORDED")
        return 0
    if a.new_private_workdir is None:
        p.error("--new-private-workdir required to run original Win95")
    run(a.private_original_archive,a.new_private_workdir)
    return 0

if __name__=="__main__":
    try:sys.exit(main())
    except (ValueError,OSError,zipfile.BadZipFile) as exc:
        print("P002_ORIGINAL_WIN95_OWNER_CAPTURE_NO_GO",exc,file=sys.stderr)
        sys.exit(2)
