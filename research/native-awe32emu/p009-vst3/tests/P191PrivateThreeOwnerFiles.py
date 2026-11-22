#!/usr/bin/env python3
"""P191: local-only three-original-asset preflight and partial native VST3 test.

This is NOT the complete P181 two-owner-SF2 test: the additional vintage.sf2
is explicitly generated from PUBLIC synthetic fixture code and is reported as
a comparison input. No owner data or staged VST3 may enter public CI/artifacts.
"""
from __future__ import annotations
import argparse
import hashlib
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

MARKERS = (
    "P018_77_EXACT_SF2_PRESETS_REAL_VST3_PCM_PASS",
    "P165_REAL_OWNER_V5_EXTERNAL_SHA_SF2_POSITIVE_REOPEN_ORIGINAL_PCM_PASS",
    "P181_PRIVATE_EXACT_WIN64_VST3_ORIGINAL_ROM_SBK_SF2_THREE_RATES_VELOCITY_KEY_RANGES_SPARSE_RECALL_PASS",
    "P010_PRIVATE_ACTUAL_WIN64_VST3_ORIGINAL_ROM_SBK_TWO_SF2_77_PRESETS_ABA_PASS",
)

def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()

def check_originals(rom: Path, sbk: Path, sf2: Path) -> None:
    if rom.stat().st_size != 1_048_576:
        raise ValueError("awe32.raw must have exactly 1048576 bytes")
    if not (0 < sbk.stat().st_size <= 64 * 1024 * 1024):
        raise ValueError("SYNTHGM.SBK is missing or exceeds 64 MiB")
    if not (12 <= sf2.stat().st_size <= 64 * 1024 * 1024):
        raise ValueError("77-preset SF2 length is invalid")
    with sf2.open("rb") as stream:
        header = stream.read(12)
    if header[:4] != b"RIFF" or header[8:] != b"sfbk":
        raise ValueError("owner SF2 lacks RIFF sfbk signature")

def require_windows_x64_pe(path: Path) -> None:
    """Reject a text stub or an x86 executable posing as the host/module."""
    with path.open("rb") as stream:
        dos = stream.read(64)
        if len(dos) != 64 or dos[:2] != b"MZ":
            raise ValueError("not a native Windows PE image: " + path.name)
        pe_offset = int.from_bytes(dos[0x3C:0x40], "little")
        if not (64 <= pe_offset <= 16 * 1024 * 1024):
            raise ValueError("invalid PE header location: " + path.name)
        stream.seek(pe_offset)
        header = stream.read(26)
    if (len(header) != 26 or header[:4] != b"PE\\0\\0" or
        int.from_bytes(header[4:6], "little") != 0x8664 or
        int.from_bytes(header[24:26], "little") != 0x20B):
        raise ValueError("expected native Windows x64 PE32+ image: " + path.name)

def run(args) -> None:
    rom = args.rom.resolve(strict=True)
    sbk = args.sbk.resolve(strict=True)
    sf2 = args.sf2.resolve(strict=True)
    bundle = args.bundle.resolve(strict=True)
    host = args.host.resolve(strict=True)
    generator = Path(__file__).resolve().with_name("make_public_layered_sf2.py")
    check_originals(rom, sbk, sf2)
    if not generator.is_file() or not host.is_file():
        raise ValueError("pinned synthetic SF2 generator or real P010 host executable missing")
    require_windows_x64_pe(host)
    module = bundle / "Contents" / "x86_64-win" / bundle.name
    if not module.is_file():
        raise ValueError("expected identically named Win64 VST3 module missing")
    require_windows_x64_pe(module)
    if bundle.is_symlink() or any(path.is_symlink() for path in bundle.rglob("*")):
        raise ValueError("linked source bundle not accepted")
    scratch = args.scratch.resolve(strict=True)
    if scratch == bundle or bundle in scratch.parents or scratch in bundle.parents:
        raise ValueError("scratch must be outside the shipping VST3 bundle")
    if any(scratch == path.parent or scratch in path.parents for path in (rom, sbk, sf2)):
        raise ValueError("original inputs must not be inside scratch")
    with tempfile.TemporaryDirectory(prefix="P191-private-", dir=scratch) as temp:
        work = Path(temp)
        cloned = work / bundle.name
        shutil.copytree(bundle, cloned)
        resources = cloned / "Contents" / "Resources" / "P009-Private"
        if resources.exists():
            shutil.rmtree(resources)
        resources.mkdir(parents=True)
        for original, name in ((rom, "awe32.raw"), (sbk, "SYNTHGM.SBK"),
                               (sf2, "77-presets.sf2")):
            staged = resources / name
            shutil.copyfile(original, staged)
            if file_sha256(staged) != file_sha256(original):
                raise RuntimeError("staged original SHA-256 mismatch: " + name)
        # This controlled alternate is not an authentic user SoundFont.
        generated = subprocess.run(
            [sys.executable, str(generator), str(resources / "vintage.sf2"), "--wet"],
            cwd=work, capture_output=True, text=True, check=False)
        if generated.returncode or not (resources / "vintage.sf2").is_file():
            raise RuntimeError("public comparison SF2 creation failed: " + generated.stderr)
        print("P191_INPUTS=OWNER_ROM+OWNER_SBK+OWNER_77SF2+PUBLIC_SYNTHETIC_COMPARISON",
              flush=True)
        result = subprocess.run([str(host), str(cloned), "--private"],
                                cwd=work, capture_output=True, text=True, check=False)
        (scratch / "P191-PRIVATE-HOST-LOG.txt").write_text(
            result.stdout + "\n" + result.stderr, encoding="utf-8")
        missing = [item for item in MARKERS if item not in result.stdout]
        if result.returncode or missing:
            raise RuntimeError(f"host exit={result.returncode}; missing gates={missing}")
        print("P191_OWNER_ROM_SBK_77SF2_NATIVE_HOST_WITH_PUBLIC_COMPARISON_PASS")
        print("P191_SECOND_AUTHENTIC_OWNER_SF2_NOT_RUN")
        print("P191_STUDIO_PRO_ASIO_PHYSICAL_HARDWARE_NOT_RUN")
    print("P191_PRIVATE_TEMPORARY_BUNDLE_CLEANED")

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("rom", "sbk", "sf2", "bundle", "host", "scratch"):
        parser.add_argument("--" + name, type=Path, required=True)
    try:
        run(parser.parse_args())
        return 0
    except (OSError, RuntimeError, ValueError) as exc:
        print("P191_NOT_PASS " + str(exc), file=sys.stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
