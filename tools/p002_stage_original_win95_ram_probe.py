#!/usr/bin/env python3
"""P002: stage ORIGINAL Creative Win95 RAM bank capture into FAT16 VM CLONE.

DO NOT put Windows 95, Creative SF2 or the original AWE32 ROM on GitHub.
This tool reads the user's already-existing PRIVATE 512-MiB Win95 disk
image read-only and creates a separate disposable FAT16 clone containing
exactly P002RAM.EXE and P002RAM.SF2 in the guest C: root.

Both MBR/FAT/Win95 original bytes and their original SHA256 remain in
owner private storage. No VM is launched here and no guest execution or
original driver RAM transfer is falsely claimed.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import shutil
import struct

ORIGINAL_DISK_BYTES = 536_870_912
GUEST_BOOTSTRAP = b"P002RAM EXE"    # Windows 95 8.3 FAT root entry
GUEST_SOUNDFONT = b"P002RAM SF2"


def require(condition: bool, problem: str):
    if not condition:
        raise ValueError(problem)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def stage(original: Path, destination: Path, win95_exe: Path,
          private_sf2: Path) -> dict:
    original = original.resolve()
    destination = destination.resolve()
    win95_exe = win95_exe.resolve()
    private_sf2 = private_sf2.resolve()
    repository = Path(__file__).resolve().parents[1]
    require(original.is_file() and
            original.stat().st_size == ORIGINAL_DISK_BYTES,
            "Expected existing private 512-MiB Windows95 FAT16 disk")
    require(win95_exe.is_file() and
            1024 < win95_exe.stat().st_size < 1_000_000,
            "Original Creative Win95 P002RAM.EXE missing/invalid")
    require(private_sf2.is_file() and
            96 <= private_sf2.stat().st_size <= 32 * 1024 * 1024,
            "Private SoundFont absent/outside checked research size")
    require(destination not in (original, win95_exe, private_sf2) and
            not destination.exists() and repository not in destination.parents,
            "Private disposable clone must be new, distinct and outside Git")
    with win95_exe.open("rb") as source:
        require(source.read(2) == b"MZ",
                "Original Creative guest executable is not Windows PE")
    with private_sf2.open("rb") as source:
        head = source.read(12)
        require(head[:4] == b"RIFF" and head[8:] == b"sfbk",
                "Private SoundFont lacks standard RIFF/sfbk identity")
        require(struct.unpack_from("<I", head, 4)[0] ==
                private_sf2.stat().st_size - 8,
                "Private SF2 RIFF length mismatch")

    # Hard private source identity and original disk safety gates.
    disk_sha = sha256(original)
    sf2_sha = sha256(private_sf2)
    exe_sha = sha256(win95_exe)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(original, destination)
    try:
        with destination.open("r+b") as disk:
            def read(at: int, length: int) -> bytes:
                disk.seek(at)
                value = disk.read(length)
                require(len(value) == length, "Truncated FAT16 clone")
                return value

            def write(at: int, blob: bytes):
                disk.seek(at)
                require(disk.write(blob) == len(blob),
                        "Partial private Win95 FAT16 image write")

            mbr = read(0, 512)
            require(mbr[510:512] == b"\x55\xAA",
                    "Private guest disk MBR signature missing")
            partition = struct.unpack_from("<I", mbr, 454)[0]
            require(partition > 0 and partition < 8192,
                    "Unexpected Win95 primary partition LBA")
            boot = read(partition * 512, 512)
            bytes_per_sector = struct.unpack_from("<H", boot, 11)[0]
            sectors_per_cluster = boot[13]
            reserved = struct.unpack_from("<H", boot, 14)[0]
            fat_copies = boot[16]
            root_entries = struct.unpack_from("<H", boot, 17)[0]
            fat_sectors = struct.unpack_from("<H", boot, 22)[0]
            sectors_total = struct.unpack_from("<I", boot, 32)[0]
            require(bytes_per_sector == 512 and
                    sectors_per_cluster == 16 and reserved == 1 and
                    fat_copies == 2 and root_entries == 512 and
                    fat_sectors == 256 and b"FAT16" in boot,
                    "Unrecognized Win95 FAT16 geometry; no guessed patching")
            require(boot[510:512] == b"\x55\xAA",
                    "Original FAT16 boot-sector signature missing")
            cluster_bytes = bytes_per_sector * sectors_per_cluster
            fat_offset = (partition + reserved) * 512
            fat_size = fat_sectors * 512
            root_start = fat_offset + 2 * fat_size
            root_bytes = root_entries * 32
            data_start = root_start + root_bytes
            cluster_count = ((sectors_total - reserved -
                              2 * fat_sectors - root_bytes // 512) //
                             sectors_per_cluster)
            require(20_000 <= cluster_count < 65_535 and
                    data_start + cluster_count * cluster_bytes <=
                    ORIGINAL_DISK_BYTES,
                    "FAT16 cluster count or extent inconsistent")
            original_fat = read(fat_offset, fat_size)
            require(read(fat_offset + fat_size, fat_size) == original_fat,
                    "Source guest FAT1/FAT2 differ; unsafe to modify")
            root = read(root_start, root_bytes)
            slots = []
            for i in range(0, root_bytes, 32):
                ent = root[i:i+32]
                if ent[0] in (0, 0xE5):
                    slots.append(i)
                elif ent[:11] in (GUEST_BOOTSTRAP, GUEST_SOUNDFONT):
                    raise ValueError("Private guest already contains P002RAM file")
            require(len(slots) >= 2, "FAT16 root has insufficient free entries")
            payloads = [(GUEST_BOOTSTRAP, win95_exe),
                        (GUEST_SOUNDFONT, private_sf2)]
            needed = [((p.stat().st_size + cluster_bytes - 1) //
                       cluster_bytes) for _, p in payloads]
            selected = []
            for cluster in range(2, cluster_count + 2):
                if struct.unpack_from("<H", original_fat,cluster*2)[0] == 0:
                    selected.append(cluster)
                    if len(selected) == sum(needed):
                        break
            require(len(selected) == sum(needed),
                    "Insufficient free guest FAT16 physical clusters")
            pos = 0
            for (name, path), count, slot in zip(payloads, needed, slots):
                chain = selected[pos:pos+count]
                pos += count
                with path.open("rb") as source:
                    for i, cluster in enumerate(chain):
                        nxt = 0xFFFF if i == count-1 else chain[i+1]
                        for fat in range(2):
                            write(fat_offset + fat * fat_size +
                                  cluster * 2, struct.pack("<H", nxt))
                        piece = source.read(cluster_bytes)
                        write(data_start+(cluster-2)*cluster_bytes,
                              piece.ljust(cluster_bytes,b"\0"))
                    require(not source.read(1), "Private source size changed")
                entry = bytearray(32)
                entry[:11] = name
                entry[11] = 0x20
                struct.pack_into("<H", entry, 26,chain[0])
                struct.pack_into("<I",entry,28,path.stat().st_size)
                write(root_start+slot,entry)
                # Check the independently read-back guest file's full
                # exact SHA256; do not trust successful FAT metadata.
                digest = hashlib.sha256()
                left = path.stat().st_size
                for cluster in chain:
                    n = min(left,cluster_bytes)
                    digest.update(read(data_start+(cluster-2)*cluster_bytes,n))
                    left -= n
                require(left == 0 and digest.hexdigest() == sha256(path),
                        "Guest FAT16 readback differs from original source")
            disk.flush()
        require(sha256(original) == disk_sha,
                "Original private Windows95 image unexpectedly modified")
        return {
            "private_clone": str(destination),
            "guest_exe": "C:\\P002RAM.EXE",
            "guest_sf2": "C:\\P002RAM.SF2",
            "win95_original_disk_sha256_unchanged": disk_sha,
            "sf2_sha256": sf2_sha,
            "guest_exe_sha256": exe_sha,
            "stage": "PRIVATE_FAT16_CLONE_READBACK_VERIFIED",
            "guest_original_driver_executed": False,
            "original_emu8000_register_audio_capture": "NOT_YET",
        }
    except BaseException:
        destination.unlink(missing_ok=True)
        raise


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--private-win95-disk",type=Path,required=True)
    parser.add_argument("--private-clone",type=Path,required=True)
    parser.add_argument("--win95-p002ram-exe",type=Path,required=True)
    parser.add_argument("--private-sf2",type=Path,required=True)
    args=parser.parse_args()
    print(stage(args.private_win95_disk,args.private_clone,
                args.win95_p002ram_exe,args.private_sf2))


if __name__=="__main__":
    main()
