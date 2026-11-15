"""Strict synthetic FAT16 test for original Creative RAM guest disk staging.

Private Windows 95 and SoundFont vendor bytes are NEVER used here.
The 512-MiB test disk is a freshly synthesized, sparse dummy FAT16
volume; no Microsoft OS, original 86Box VM, Creative DLL or ROM inside.
"""
from __future__ import annotations
import hashlib
from pathlib import Path
import struct
import tempfile
import unittest

from tools import p002_stage_original_win95_ram_probe as stage

def synthetic_disk(path: Path):
    with path.open("wb") as f:
        f.truncate(stage.ORIGINAL_DISK_BYTES)
    mbr = bytearray(512)
    mbr[450] = 0x06  # MBR FAT16 LBA partition type
    struct.pack_into("<I",mbr,454,63)
    mbr[510:512] = b"\x55\xAA"
    boot = bytearray(512)
    struct.pack_into("<H",boot,11,512)
    boot[13] = 16
    struct.pack_into("<H",boot,14,1)
    boot[16] = 2
    struct.pack_into("<H",boot,17,512)
    struct.pack_into("<H",boot,22,256)
    struct.pack_into("<I",boot,32,1_039_000)
    boot[54:62] = b"FAT16   "
    boot[510:512] = b"\x55\xAA"
    first_fat = (63+1)*512
    with path.open("r+b") as f:
        f.write(mbr)
        f.seek(63*512)
        f.write(boot)
        for copy in (0,1):
            f.seek(first_fat+copy*256*512)
            f.write(b"\xF8\xFF\xFF\xFF")

def write_inputs(directory: Path):
    old = directory/"private-synthetic-win95.img"
    exe = directory/"P002RAM.EXE"
    sf2 = directory/"private-test-bank.sf2"
    synthetic_disk(old)
    exe.write_bytes(b"MZ"+bytes([i%251 for i in range(6500)]))
    tail=bytes([i%249 for i in range(18000)])
    sf2.write_bytes(b"RIFF"+struct.pack("<I",4+len(tail))+
                    b"sfbk"+tail)
    return old,exe,sf2

class OriginalCreativeGuestStageTests(unittest.TestCase):
    def test_private_512mb_synthetic_guest_clone_and_exact_readback(self):
        with tempfile.TemporaryDirectory(prefix="P002-synthetic-") as td:
            root=Path(td)
            original,exe,sf2=write_inputs(root)
            digest=stage.sha256(original)
            clone=root/"PRIVATE_CLONE.img"
            report=stage.stage(original,clone,exe,sf2)
            self.assertTrue(clone.exists())
            self.assertEqual(clone.stat().st_size,
                             stage.ORIGINAL_DISK_BYTES)
            self.assertEqual(stage.sha256(original),digest)
            self.assertEqual(report["win95_original_disk_sha256_unchanged"],
                             digest)
            self.assertEqual(report["guest_exe"],"C:\\P002RAM.EXE")
            self.assertEqual(report["guest_sf2"],"C:\\P002RAM.SF2")
            self.assertFalse(report["guest_original_driver_executed"])
            self.assertEqual(
                report["stage"],"PRIVATE_FAT16_CLONE_READBACK_VERIFIED")
            self.assertEqual(report["sf2_sha256"],
                             hashlib.sha256(sf2.read_bytes()).hexdigest())
            # Both exact 8.3 names are present, original OS image is
            # untouched, and both FAT copies agree after allocation.
            with clone.open("rb") as f:
                f.seek((63+1)*512)
                fat1=f.read(256*512)
                fat2=f.read(256*512)
                self.assertEqual(fat1,fat2)
                root_base=(63+1+512)*512
                f.seek(root_base)
                listing=f.read(512*32)
                self.assertEqual(listing[:11],b"P002RAM EXE")
                self.assertEqual(listing[32:43],b"P002RAM SF2")
            with self.assertRaisesRegex(ValueError,"must be new"):
                stage.stage(original,clone,exe,sf2)

    def test_optional_second_original_bank_staged_and_sha_verified(self):
        with tempfile.TemporaryDirectory(prefix="P003-two-banks-") as td:
            root=Path(td)
            original,exe,sf2=write_inputs(root)
            second=root/"other-independent.sf2"
            data=bytearray(sf2.read_bytes())
            data[-1]^=0x5A
            second.write_bytes(data)
            before=stage.sha256(original)
            clone=root/"separate-two-bank-guest.img"
            result=stage.stage(original,clone,exe,sf2,second)
            self.assertEqual(result["guest_second_sf2"],"C:\\P003BANK.SF2")
            self.assertEqual(result["second_sf2_sha256"],
                hashlib.sha256(second.read_bytes()).hexdigest())
            self.assertNotEqual(result["second_sf2_sha256"],
                                result["sf2_sha256"])
            self.assertFalse(result["guest_original_driver_executed"])
            self.assertEqual(stage.sha256(original),before)
            with clone.open("rb") as disk:
                disk.seek((63+1)*512)
                self.assertEqual(disk.read(256*512),
                                 disk.read(256*512))
                disk.seek((63+1+512)*512)
                root_entries=disk.read(3*32)
                self.assertEqual(root_entries[:11],b"P002RAM EXE")
                self.assertEqual(root_entries[32:43],b"P002RAM SF2")
                self.assertEqual(root_entries[64:75],b"P003BANKSF2")
            with self.assertRaisesRegex(ValueError,"same source|must differ"):
                stage.stage(original,root/"reject-same.img",exe,sf2,sf2)
            self.assertFalse((root/"reject-same.img").exists())

    def test_bad_bank_rejected_before_private_image_copy(self):
        with tempfile.TemporaryDirectory(prefix="P002-synthetic-") as td:
            root=Path(td)
            original,exe,sf2=write_inputs(root)
            before=stage.sha256(original)
            bank=bytearray(sf2.read_bytes())
            bank[8:12]=b"NOPE"
            sf2.write_bytes(bank)
            clone=root/"must-not-exist.img"
            with self.assertRaisesRegex(ValueError,"RIFF/sfbk"):
                stage.stage(original,clone,exe,sf2)
            self.assertFalse(clone.exists())
            self.assertEqual(stage.sha256(original),before)

if __name__=="__main__":
    unittest.main()
