#!/usr/bin/env python3
"""Insert original-driver P001MID.EXE into a PRIVATE DISPOSABLE Win95 FAT16 clone.

No Microsoft/Creative binaries and no user disk/ROM/SoundFont enter GitHub.
The source image is opened read-only and never edited in place.
"""
from __future__ import annotations
import argparse
import hashlib
import math
from pathlib import Path
import shutil
import struct


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def stage(original: Path, destination: Path, executable: Path) -> dict:
    original = original.resolve()
    destination = destination.resolve()
    executable = executable.resolve()
    require(original.is_file() and original.stat().st_size == 536870912,
            "Expected existing private 512-MiB Windows 95 FAT16 disk image")
    require(executable.is_file() and 2048 < executable.stat().st_size < 1000000,
            "Win95 PE32 probe absent or implausible length")
    require(original != destination and not destination.exists() and
            original not in destination.parents,
            "Refuse to overwrite original/private guest image")
    payload = executable.read_bytes()
    require(payload[:2] == b"MZ", "Not a Win95 executable image")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(original, destination)
    with destination.open("r+b") as disk:
        def read(offset: int, size: int) -> bytes:
            disk.seek(offset)
            return disk.read(size)

        def write(offset: int, data: bytes) -> None:
            disk.seek(offset)
            require(disk.write(data) == len(data), "Incomplete Win95 FAT write")

        start_sector = struct.unpack("<I", read(454, 4))[0]
        sector = read(start_sector * 512, 512)
        bytes_per_sector = struct.unpack_from("<H", sector, 11)[0]
        sectors_per_cluster = sector[13]
        reserved = struct.unpack_from("<H", sector, 14)[0]
        fat_copies = sector[16]
        root_entries = struct.unpack_from("<H", sector, 17)[0]
        fat_sectors = struct.unpack_from("<H", sector, 22)[0]
        total_sectors = struct.unpack_from("<I", sector, 32)[0]
        require(bytes_per_sector == 512 and sectors_per_cluster == 16 and
                reserved == 1 and fat_copies == 2 and root_entries == 512 and
                fat_sectors == 256 and b"FAT16" in sector,
                "Unexpected guest image/FAT layout; refusing unsafe modification")
        cluster_bytes = bytes_per_sector * sectors_per_cluster
        first_fat = (start_sector + reserved) * bytes_per_sector
        root_offset = ((start_sector + reserved +
                        fat_copies * fat_sectors) * bytes_per_sector)
        root_size = root_entries * 32
        data_offset = root_offset + root_size
        cluster_count = ((total_sectors - reserved - fat_copies * fat_sectors -
                          root_size // bytes_per_sector) // sectors_per_cluster)
        require(20000 <= cluster_count < 65535, "Unexpected FAT16 cluster count")
        require(data_offset + cluster_count * cluster_bytes <= destination.stat().st_size,
                "Filesystem claims data beyond private guest disk")
        entries = read(root_offset, root_size)
        marker = b"P001MID EXE"
        free_root = None
        for offset in range(0, root_size, 32):
            if entries[offset] in (0, 0xE5):
                if free_root is None:
                    free_root = offset
            else:
                require(entries[offset:offset + 11].upper() != marker,
                        "Refuse to overwrite an existing Win95 P001MID.EXE")
        require(free_root is not None, "No free Win95 FAT16 root entry")
        fat = read(first_fat, fat_sectors * bytes_per_sector)
        needed = math.ceil(len(payload) / cluster_bytes)
        selected = []
        for cluster in range(2, cluster_count + 2):
            if len(selected) >= needed:
                break
            if struct.unpack_from("<H", fat, cluster * 2)[0] == 0:
                selected.append(cluster)
        require(len(selected) == needed, "Not enough safe free FAT16 clusters")
        for index, cluster in enumerate(selected):
            next_cluster = 0xFFFF if index == len(selected) - 1 else selected[index + 1]
            for copy in range(fat_copies):
                write(first_fat + copy * fat_sectors * bytes_per_sector + cluster * 2,
                      struct.pack("<H", next_cluster))
            chunk = payload[index * cluster_bytes:(index + 1) * cluster_bytes]
            write(data_offset + (cluster - 2) * cluster_bytes,
                  chunk.ljust(cluster_bytes, b"\0"))
        directory_entry = bytearray(32)
        directory_entry[:11] = marker
        directory_entry[11] = 0x20
        struct.pack_into("<H", directory_entry, 26, selected[0])
        struct.pack_into("<I", directory_entry, 28, len(payload))
        write(root_offset + free_root, directory_entry)
        require(read(root_offset + free_root, 11) == marker,
                "Win95 FAT root entry verification failed")
        recovered = b"".join(
            read(data_offset + (cluster - 2) * cluster_bytes, cluster_bytes)
            for cluster in selected
        )[:len(payload)]
        require(recovered == payload,
                "Win95 FAT16 file differs after private disk write")
    return {
        "private_win95_file": r"C:\P001MID.EXE",
        "guest_probe_sha256": hashlib.sha256(payload).hexdigest(),
        "guest_probe_size": len(payload),
        "allocated_clusters": len(selected),
        "source_disk_unmodified_by_staging": True,
        "guest_execution_verified": False,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--dest", type=Path, required=True)
    parser.add_argument("--exe", type=Path, required=True)
    args = parser.parse_args()
    print(stage(args.source, args.dest, args.exe))


if __name__ == "__main__":
    main()
