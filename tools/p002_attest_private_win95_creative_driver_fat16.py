#!/usr/bin/env python3
"""Offline, READ-ONLY SHA256 attestation of EXISTING private Win95 Creative files.

This is NOT a VM boot, installed-driver execution, bank selection, or
original EMU8000 RAM/PCM verification. Supplied expected hashes MUST come
from the owner's separately authenticated driver reference; merely
agreeing with themselves does not prove historical Creative origin.
The private Microsoft disk, Creative DLL/VXD and SoundFont never leave
the owner's machine or enter public GitHub CI.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

try:
    from .p002_extract_original_win95_ram_log_fat16 import (
        PrivateFat16Reader, sha256_file, MAX_BANK)
except ImportError:
    from p002_extract_original_win95_ram_log_fat16 import (
        PrivateFat16Reader, sha256_file, MAX_BANK)

WINDOWS = b"WINDOWS    "
SYSTEM = b"SYSTEM     "
AWE32 = b"AWE32      "
SF2 = b"SF2        "
AWEMAN32 = b"AWEMAN32DLL"
SBAWE = b"SBAWE   VXD"
SYNTH512 = b"SYNTH512SF2"
MAX_DLL = 1_000_000
MAX_VXD = 2_000_000
HEX256 = re.compile(r"[0-9a-fA-F]{64}\Z")


def sha(blob: bytes) -> str:
    return hashlib.sha256(blob).hexdigest()


def attest(private_disk: Path, expected_vxd_sha256: str,
           expected_aweman_sha256: str,
           private_bank_sf2: Path | None = None) -> dict:
    if (not HEX256.fullmatch(expected_vxd_sha256) or
            not HEX256.fullmatch(expected_aweman_sha256)):
        raise ValueError(
            "Both independently supplied original Creative SHA256 "
            "reference strings must contain exactly 64 hex digits")
    expected_vxd_sha256 = expected_vxd_sha256.lower()
    expected_aweman_sha256 = expected_aweman_sha256.lower()
    with PrivateFat16Reader(private_disk) as disk:
        # Windows 95 searches multiple directories for AWEMAN32.
        # A mismatched shadow copy could invalidate a source reference
        # even if one hand-selected DLL were correct.
        dll_windows = disk.read_short_path_file(
            (WINDOWS, AWEMAN32), MAX_DLL)
        dll_system = disk.read_short_path_file(
            (WINDOWS, SYSTEM, AWEMAN32), MAX_DLL)
        vxd_system = disk.read_short_path_file(
            (WINDOWS, SYSTEM, SBAWE), MAX_VXD)
        vxd_vendor = disk.read_short_path_file(
            (AWE32, SBAWE), MAX_VXD)
        if sha(dll_windows) != sha(dll_system):
            raise ValueError(
                "Win95 AWEMAN32.DLL shadow copy differs from SYSTEM")
        if sha(vxd_vendor) != sha(vxd_system):
            raise ValueError(
                "Win95 SBAWE.VXD differs from private Creative install copy")
        if sha(dll_system) != expected_aweman_sha256:
            raise ValueError(
                "Private Win95 installed AWEMAN32.DLL SHA256 differs "
                "from independently supplied source")
        if sha(vxd_system) != expected_vxd_sha256:
            raise ValueError(
                "Private Win95 installed SBAWE.VXD SHA256 differs "
                "from independently supplied source")
        result = {
            "status": "P002_PRIVATE_WIN95_INSTALLED_CREATIVE_FILE_SHA256_MATCH",
            "guest_disk_sha256": sha256_file(disk.image),
            "aweman32_sha256": sha(dll_system),
            "sbawe_vxd_sha256": sha(vxd_system),
            "guest_windows_and_system_aweman_identical": True,
            "guest_system_and_vendor_sbawe_identical": True,
            "reference_hashes_supplied_externally": True,
            "reference_source_origin_independently_authenticated":
                "MUST_BE_VERIFIED_SEPARATELY_BY_OWNER",
            "actual_win95_Creative_dll_vxd_execution":
                "NOT_VERIFIED_BY_OFFLINE_FILE_ATTESTATION",
            "original_EMU8000_ram_sf2_pcm":
                "NOT_VERIFIED_BY_OFFLINE_FILE_ATTESTATION",
            "release_approved": False,
        }
        if private_bank_sf2 is not None:
            original_bank = private_bank_sf2.resolve()
            if not (original_bank.is_file() and
                    96 <= original_bank.stat().st_size <= MAX_BANK):
                raise ValueError("Private owner source SF2 missing or oversized")
            source_bytes = original_bank.read_bytes()
            if (source_bytes[:4] != b"RIFF"
                    or source_bytes[8:12] != b"sfbk"
                    or int.from_bytes(source_bytes[4:8],"little")
                    != len(source_bytes)-8):
                raise ValueError("Private owner source is not valid RIFF SF2")
            staged = disk.read_short_path_file(
                (SF2, SYNTH512), MAX_BANK)
            if source_bytes != staged:
                raise ValueError(
                    "Private Win95 C:\\SF2\\SYNTH512.SF2 differs "
                    "from owner source bank")
            result["guest_staged_synth512_sf2_sha256"] = sha(source_bytes)
            result["guest_staged_synth512_source_bank_identical"] = True
            result["original_creative_bank_runtime_loaded"] = False
        else:
            result["guest_staged_synth512_source_bank_identical"] = (
                "NOT_CHECKED_NO_OWNER_SOURCE_SUPPLIED")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--private-win95-disk", type=Path, required=True)
    parser.add_argument("--expected-original-sbawe-vxd-sha256", required=True)
    parser.add_argument("--expected-original-aweman32-dll-sha256", required=True)
    parser.add_argument("--private-synth512-sf2", type=Path)
    parser.add_argument("--private-report", type=Path, required=True)
    args = parser.parse_args()
    destination = args.private_report.resolve()
    repository = Path(__file__).resolve().parents[1]
    protected = [args.private_win95_disk.resolve()]
    if args.private_synth512_sf2:
        protected.append(args.private_synth512_sf2.resolve())
    if (destination.exists() or destination in protected or
            destination == repository or repository in destination.parents):
        parser.error("Report must be a NEW private file outside Git/inputs")
    proof = attest(
        args.private_win95_disk,
        args.expected_original_sbawe_vxd_sha256,
        args.expected_original_aweman32_dll_sha256,
        args.private_synth512_sf2)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("x", encoding="ascii") as output:
        json.dump(proof, output, indent=2, sort_keys=True)
        output.write("\n")
    print("P002_PRIVATE_WIN95_ORIGINAL_CREATIVE_INSTALLED_FILE_HASHES_PASS",
          "guest_win95_driver_EXECUTION=NOT_TESTED",
          "original_RAM_SF2_AUDIO=NOT_PROVEN",
          "release_approved=FALSE")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError) as error:
        print("P002_PRIVATE_WIN95_DRIVER_FILE_HASH_NO_GO", error,
              file=sys.stderr)
        sys.exit(1)
