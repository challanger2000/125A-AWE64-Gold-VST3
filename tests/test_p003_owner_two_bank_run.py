"""P003 one-click Win95 cloned-disk packaging tests (NO proprietary media).

Synthetic 512MiB FAT16 on sparse disk; no actual Windows/Creative or VM.
The positive case only proves clone mutation and exact FAT readback.
"""
from __future__ import annotations
import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))

from test_p002_win95_ram_stage import write_inputs
import p002_stage_original_win95_ram_probe as stager
from p002_extract_original_win95_ram_log_fat16 import PrivateFat16Reader
from p003_private_fat16_replace import patch_disposable_guest
from p003_owner_two_bank_run import generated_comparison_sf2

def complete_synthetic_partition_mbr(original:Path):
    # The legacy stager's synthetic FAT test does not populate the
    # MBR partition length; the stricter original-owner FAT16 reader
    # requires it. Set it on THIS SYNTHETIC test fixture only.
    with original.open("r+b") as disk:
        disk.seek(458)
        disk.write(struct.pack("<I",1_039_000))

def make_existing_winini(clone:Path):
    """Only a dummy Windows directory and Win.INI for preloaded owner VM."""
    with PrivateFat16Reader(clone) as disk:
        fat=bytearray(disk.fat)
        root=bytearray(disk.root)
        free=[i for i in range(2,disk.cluster_count+2)
              if struct.unpack_from("<H",fat,i*2)[0]==0][:2]
        assert len(free)==2
        directory_cluster,ini_cluster=free
        for cluster in free:struct.pack_into("<H",fat,cluster*2,0xffff)
        directory_at=disk.data_start+(directory_cluster-2)*disk.cluster_bytes
        ini_at=disk.data_start+(ini_cluster-2)*disk.cluster_bytes
        ini=b"[windows]\r\nrun=C:\\P002RAM.EXE\r\n"
        dirent=bytearray(32)
        dirent[:11]=b"WINDOWS    "
        dirent[11]=0x10
        struct.pack_into("<H",dirent,26,directory_cluster)
        root[64:96]=dirent
        ini_entry=bytearray(32)
        ini_entry[:11]=b"WIN     INI"
        ini_entry[11]=0x20
        struct.pack_into("<H",ini_entry,26,ini_cluster)
        struct.pack_into("<I",ini_entry,28,len(ini))
        first_fat=disk.first_fat;fat_size=disk.fat_size;root_start=disk.root_start
        cluster_bytes=disk.cluster_bytes
    with clone.open("r+b") as disk:
        for j in (0,1):
            disk.seek(first_fat+j*fat_size)
            disk.write(fat)
        disk.seek(root_start);disk.write(root)
        disk.seek(directory_at);disk.write(ini_entry.ljust(cluster_bytes,b"\x00"))
        disk.seek(ini_at);disk.write(ini.ljust(cluster_bytes,b"\x00"))

class PrivateOwnerP003TwoBankTests(unittest.TestCase):
    def test_generated_bank_has_one_real_program_and_distinct_pcm(self):
        a=generated_comparison_sf2()
        b=generated_comparison_sf2()
        self.assertEqual(a,b)
        self.assertEqual(a[:4],b"RIFF")
        self.assertEqual(int.from_bytes(a[4:8],"little"),len(a)-8)
        self.assertEqual(a[8:12],b"sfbk")
        self.assertIn(b"P003-SAMPLE",a)
        self.assertIn(b"P003-GUEST",a)
        self.assertIn(b"smpl",a)
        self.assertIn(b"pmod",a)
        self.assertIn(b"imod",a)
        self.assertGreater(len(a),65000)

    def test_original_vm_clone_alone_is_modified_and_both_banks_read_back(self):
        with tempfile.TemporaryDirectory(prefix="P003-clone-test-") as td:
            root=Path(td)
            original,old_exe,first=write_inputs(root)
            complete_synthetic_partition_mbr(original)
            original_sha=stager.sha256(original)
            clone=root/"cloned-only.img"
            stager.stage(original,clone,old_exe,first)
            make_existing_winini(clone)
            original_clone_guest=old_exe.read_bytes()
            original_bank=first.read_bytes()
            second=generated_comparison_sf2()
            new_guest=b"MZ"+bytes(i%251 for i in range(25000))
            result=patch_disposable_guest(
                clone,new_guest,second,
                hashlib.sha256(original_bank).hexdigest())
            self.assertEqual(result["status"],
                             "P003_PRIVATE_DISPOSABLE_TWO_BANK_GUEST_STAGED")
            self.assertEqual(stager.sha256(original),original_sha)
            with PrivateFat16Reader(clone) as disk:
                self.assertEqual(
                    disk.read_root_file(b"P002RAM EXE",1_000_000),new_guest)
                self.assertEqual(
                    disk.read_root_file(b"P002RAM SF2",32*1024*1024),
                    original_bank)
                self.assertEqual(
                    disk.read_root_file(b"P003BANKSF2",32*1024*1024),
                    second)
                self.assertNotEqual(
                    disk.read_root_file(b"P002RAM EXE",1_000_000),
                    original_clone_guest)
                self.assertIn(
                    b"run=C:\\P002RAM.EXE",
                    disk.read_short_path_file(
                        (b"WINDOWS    ",b"WIN     INI"),32768))
            with self.assertRaisesRegex(ValueError,
                                        "existing second SF2|stale"):
                patch_disposable_guest(
                    clone,new_guest,second,
                    hashlib.sha256(original_bank).hexdigest())
            self.assertEqual(stager.sha256(original),original_sha)

    def test_mismatched_original_sf2_hash_refuses_before_any_FAT_write(self):
        with tempfile.TemporaryDirectory(prefix="P003-negative-") as td:
            root=Path(td)
            source,old_exe,first=write_inputs(root)
            complete_synthetic_partition_mbr(source)
            clone=root/"clone.img"
            stager.stage(source,clone,old_exe,first)
            make_existing_winini(clone)
            before=stager.sha256(clone)
            with self.assertRaisesRegex(ValueError,
                                        "first original Win95 SoundFont hash"):
                patch_disposable_guest(
                    clone,b"MZ"+b"x"*5000,
                    generated_comparison_sf2(),"0"*64)
            self.assertEqual(stager.sha256(clone),before)

if __name__=="__main__":
    unittest.main()
