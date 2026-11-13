#!/usr/bin/env python3
"""Read a P002RAM.LOG from an ALREADY BOOTED private Win95 FAT16 clone.

The source FAT16 disk is always opened read-only. This program does
not boot Windows 95, run Creative code, or claim original RAM upload.
It closes the missing handoff: extract the guest's actual C:\\P002RAM.LOG
from the FAT16 root without requiring Windows 95 file sharing, verify
that C:\\P002RAM.SF2 is identical to the original *private* source SF2,
then apply the strict source Creative guest API log audit.

No Microsoft, Creative, 86Box VM or user SoundFont files are committed.
Private log and report outputs MUST be NEW files outside the repository.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

try:
    from .p002_verify_original_win95_ram_guest_log import audit
except ImportError:
    from p002_verify_original_win95_ram_guest_log import audit

DISK_BYTES = 536_870_912
SECTOR_BYTES = 512
RAM_LOG_NAME = b"P002RAM LOG"
RAM_SF2_NAME = b"P002RAM SF2"
RAM_EXE_NAME = b"P002RAM EXE"
MAX_LOG = 128 * 1024
MAX_BANK = 32 * 1024 * 1024
FAT_BAD = 0xFFF7
FAT_RESERVED_START = 0xFFF0
FAT_EOC = 0xFFF8


def require(ok: bool, description: str):
    if not ok:
        raise ValueError(description)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


class PrivateFat16Reader:
    def __init__(self, image: Path):
        self.image = image.resolve()
        require(self.image.is_file() and
                self.image.stat().st_size == DISK_BYTES,
                "Private original Win95 clone must be exactly 512 MiB")
        self.file = self.image.open("rb")
        try:
            self._initialize()
        except BaseException:
            self.file.close()
            raise

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        self.file.close()

    def _read(self, offset: int, length: int):
        require(0 <= offset <= DISK_BYTES and
                0 <= length <= DISK_BYTES - offset,
                "Private FAT16 read outside original clone")
        self.file.seek(offset)
        data = self.file.read(length)
        require(len(data) == length, "Truncated private FAT16 clone")
        return data

    def _initialize(self):
        mbr = self._read(0, 512)
        require(mbr[510:512] == b"\x55\xAA",
                "Original Win95 private MBR signature missing")
        entry = mbr[446:462]
        partition_type = entry[4]
        require(partition_type in (0x04, 0x06, 0x0E),
                "Win95 primary volume is not FAT16")
        sector = struct.unpack_from("<I", entry, 8)[0]
        volume_sectors = struct.unpack_from("<I", entry, 12)[0]
        require(0 < sector < 8192 and volume_sectors > 0 and
                (sector + volume_sectors) * SECTOR_BYTES <= DISK_BYTES,
                "Win95 primary FAT16 partition invalid")
        boot = self._read(sector * 512, 512)
        require(boot[510:512] == b"\x55\xAA",
                "Win95 FAT16 boot sector invalid")
        bps = struct.unpack_from("<H", boot, 11)[0]
        spc = boot[13]
        reserved = struct.unpack_from("<H", boot, 14)[0]
        copies = boot[16]
        root_count = struct.unpack_from("<H", boot, 17)[0]
        fat_sectors = struct.unpack_from("<H", boot, 22)[0]
        total16 = struct.unpack_from("<H", boot, 19)[0]
        total32 = struct.unpack_from("<I", boot, 32)[0]
        total = total16 if total16 else total32
        require(bps == 512 and spc == 16 and reserved == 1 and
                copies == 2 and root_count == 512 and
                fat_sectors == 256 and b"FAT16" in boot,
                "Unknown FAT16 Win95 geometry; refusing guesswork")
        require(total > 0 and total <= volume_sectors,
                "Win95 FAT16 filesystem extends beyond partition")
        self.cluster_bytes = spc * bps
        self.first_fat = (sector + reserved) * bps
        self.fat_size = fat_sectors * bps
        self.root_start = self.first_fat + copies * self.fat_size
        self.root_bytes = root_count * 32
        self.data_start = self.root_start + self.root_bytes
        self.cluster_count = (
            total - reserved - copies * fat_sectors -
            self.root_bytes // bps
        ) // spc
        require(20_000 <= self.cluster_count < 65_535 and
                self.data_start +
                self.cluster_count * self.cluster_bytes <=
                (sector + volume_sectors) * SECTOR_BYTES,
                "FAT16 cluster count/extent inconsistent with Win95")
        fat1 = self._read(self.first_fat, self.fat_size)
        fat2 = self._read(self.first_fat + self.fat_size, self.fat_size)
        require(fat1 == fat2, "Private Win95 FAT1 and FAT2 disagree")
        require(fat1[:4] == b"\xF8\xFF\xFF\xFF",
                "Private FAT16 reserved/media words invalid")
        self.fat = fat1
        self.root = self._read(self.root_start, self.root_bytes)

    def read_root_file(self, short_name: bytes, max_bytes: int):
        require(len(short_name) == 11 and
                short_name.isascii() and max_bytes > 0,
                "Unexpected FAT16 8.3 target")
        entries = []
        for offset in range(0, self.root_bytes, 32):
            entry = self.root[offset:offset + 32]
            if entry[0] == 0:  # FAT: no later active root entries
                break
            if entry[0] == 0xE5 or entry[11] == 0x0F:
                continue
            if entry[:11] == short_name:
                entries.append(entry)
        require(len(entries) == 1,
                "Missing or duplicated guest FAT16 " +
                short_name.decode("ascii"))
        entry = entries[0]
        require(entry[11] & 0x18 == 0 and
                entry[11] & 0x80 == 0,
                "Guest FAT16 file is a directory, volume or invalid")
        size = struct.unpack_from("<I", entry, 28)[0]
        require(0 < size <= max_bytes,
                "Guest FAT16 file missing, oversized or empty")
        start = struct.unpack_from("<H", entry, 26)[0]
        count = (size + self.cluster_bytes - 1) // self.cluster_bytes
        require(2 <= start < self.cluster_count + 2,
                "Guest FAT16 file starts outside installed volume")
        visited = set()
        data = bytearray()
        cluster = start
        for index in range(count):
            require(2 <= cluster < self.cluster_count + 2 and
                    cluster not in visited,
                    "Guest FAT16 file has cycle/out-of-range chain")
            visited.add(cluster)
            offset = self.data_start + (
                cluster - 2) * self.cluster_bytes
            take = min(self.cluster_bytes, size - len(data))
            data.extend(self._read(offset, take))
            following = struct.unpack_from(
                "<H", self.fat, cluster * 2)[0]
            if index + 1 == count:
                require(FAT_EOC <= following <= 0xFFFF,
                        "Guest FAT16 file chain does not end exactly at EOF")
            else:
                require(2 <= following < self.cluster_count + 2 and
                        following < FAT_RESERVED_START,
                        "Guest FAT16 file chain truncated/invalid")
                cluster = following
        require(len(data) == size, "Guest FAT16 readback incomplete")
        return bytes(data)


def extract_guest_ram_evidence(clone: Path, original_sf2: Path):
    original_sf2 = original_sf2.resolve()
    require(original_sf2.is_file() and
            96 <= original_sf2.stat().st_size <= MAX_BANK,
            "Expected the privately owned original SF2 bank")
    sf2 = original_sf2.read_bytes()
    with PrivateFat16Reader(clone) as disk:
        original_guest_sf2 = disk.read_root_file(RAM_SF2_NAME, MAX_BANK)
        require(original_guest_sf2 == sf2,
                "Guest C:\\P002RAM.SF2 differs from ORIGINAL private SF2 bytes")
        _ = disk.read_root_file(RAM_EXE_NAME, 1_000_000)
        log = disk.read_root_file(RAM_LOG_NAME, MAX_LOG)
        evaluated = audit(log, sf2)
        result = {
            **evaluated,
            "private_win95_clone_sha256": sha256_file(disk.image),
            "private_win95_log_extracted_from_fat16_root": True,
            "guest_sf2_entire_content_identical_to_private_source": True,
            "actual_original_emu8000_RAM_upload_trace": "NOT_VERIFIED_BY_GUEST_LOG",
            "actual_original_emu8000_dry_PCM": "NOT_VERIFIED_BY_GUEST_LOG",
            "release_approved": False,
        }
    return log, result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--private-booted-win95-clone", type=Path, required=True)
    parser.add_argument("--private-sf2", type=Path, required=True)
    parser.add_argument("--private-extracted-log", type=Path, required=True)
    parser.add_argument("--private-report", type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    outputs = [args.private_extracted_log.resolve(),
               args.private_report.resolve()]
    if len(set(outputs)) != len(outputs) or any(
        p.exists() or p == repo or repo in p.parents or
        p == args.private_booted_win95_clone.resolve() or
        p == args.private_sf2.resolve()
        for p in outputs
    ):
        parser.error("Private output files must be new, distinct and outside Git")
    log, report = extract_guest_ram_evidence(
        args.private_booted_win95_clone, args.private_sf2)
    # Read-only source VM and private bank. Publish neither to GitHub.
    for output in outputs:
        output.parent.mkdir(parents=True, exist_ok=True)
    with outputs[0].open("xb") as destination:
        destination.write(log)
    with outputs[1].open("x", encoding="utf-8") as destination:
        json.dump(report, destination, indent=2)
        destination.write("\n")
    print("P002_PRIVATE_FAT16_ORIGINAL_CREATIVE_RAM_LOG_EXTRACTED_PASS",
          "sf2_source_identical=TRUE",
          "driver_EMU8000_RAM_port_trace=NOT_YET_VERIFIED",
          "original_DRY_PCM=NOT_YET_VERIFIED",
          "release_approved=FALSE")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError) as error:
        print("P002_PRIVATE_FAT16_RAM_LOG_EXTRACTION_NO_GO",
              error, file=sys.stderr)
        sys.exit(1)
