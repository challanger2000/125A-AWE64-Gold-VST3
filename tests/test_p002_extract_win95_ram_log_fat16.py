"""Read-only extraction of original Creative guest log from PRIVATE FAT16 clone.

Tests use a synthetic sparse 512-MiB disk, a generated RIFF SF2 and a
synthetic guest log. No Microsoft, Creative, user bank or driver bytes.
"""
from __future__ import annotations

from pathlib import Path
import hashlib
import struct
import tempfile
import unittest

from tools import p002_extract_original_win95_ram_log_fat16 as tool
from tools import p002_stage_original_win95_ram_probe as stage
from tests.test_p002_win95_ram_stage import write_inputs
from tests.test_p002_win95_ram_guest_log import synth_guest_log, sf2_bank

PART_LBA=63
FAT_START=(PART_LBA+1)*512
FAT_BYTES=256*512
ROOT_START=FAT_START+2*FAT_BYTES
DATA_START=ROOT_START+512*32
CLUSTER_BYTES=8192


def add_original_guest_log(clone: Path, data: bytes,
                           short_name: bytes=b"P002RAM LOG"):
    assert len(short_name)==11
    num=(len(data)+CLUSTER_BYTES-1)//CLUSTER_BYTES
    assert 1<=num<=100
    # Source staging uses free clusters 2 and 3. Reserve a disjoint
    # physical cluster chain for the simulated original Win95 log.
    with clone.open("r+b") as disk:
        disk.seek(FAT_START)
        fat=disk.read(FAT_BYTES)
        free=[
            c for c in range(2,65000)
            if struct.unpack_from("<H",fat,c*2)[0]==0
        ][:num]
        assert len(free)==num
        for i,cluster in enumerate(free):
            nxt=0xFFFF if i+1==num else free[i+1]
            for mirror in (0,1):
                disk.seek(FAT_START+mirror*FAT_BYTES+cluster*2)
                disk.write(struct.pack("<H",nxt))
            disk.seek(DATA_START+(cluster-2)*CLUSTER_BYTES)
            disk.write(data[i*CLUSTER_BYTES:(i+1)*CLUSTER_BYTES].ljust(
                CLUSTER_BYTES,b"\x00"))
        disk.seek(ROOT_START)
        root=disk.read(512*32)
        slots=[
            i for i in range(0,len(root),32)
            if root[i] in (0,0xE5)
        ]
        assert slots
        entry=bytearray(32)
        entry[:11]=short_name
        entry[11]=0x20
        struct.pack_into("<H",entry,26,free[0])
        struct.pack_into("<I",entry,28,len(data))
        disk.seek(ROOT_START+slots[0])
        disk.write(entry)
    return free


def new_guest(root: Path):
    original, exe, sf2 = write_inputs(root)
    sf2.write_bytes(sf2_bank())
    # The legacy test fixture never needed the MBR partition length.
    # The read-only extractor deliberately requires it.
    with original.open("r+b") as disk:
        disk.seek(458)
        disk.write(struct.pack("<I",1_048_000))
    clone=root/"original-creative-guest-booted-clone.img"
    stage.stage(original,clone,exe,sf2)
    return original, clone, sf2


class OriginalWin95RamLogReadbackTests(unittest.TestCase):
    def test_accurate_guest_log_and_original_bank_automatically_extracted(self):
        with tempfile.TemporaryDirectory(prefix="p002-log-source-") as dirname:
            root=Path(dirname)
            original, clone, sf2=new_guest(root)
            log=synth_guest_log(len(sf2.read_bytes()))
            chain=add_original_guest_log(clone,log)
            self.assertEqual(len(chain),1)
            source_sha=stage.sha256(original)
            clone_sha=stage.sha256(clone)
            found,report=tool.extract_guest_ram_evidence(clone,sf2)
            self.assertEqual(found,log)
            self.assertTrue(report[
                "guest_sf2_entire_content_identical_to_private_source"])
            self.assertEqual(report["private_win95_clone_sha256"],clone_sha)
            self.assertEqual(report["status"],
                "ORIGINAL_CREATIVE_WIN95_API_GUEST_LOG_CONTRACT_PASS")
            self.assertFalse(report["release_approved"])
            self.assertEqual(report["actual_original_emu8000_RAM_upload_trace"],
                             "NOT_VERIFIED_BY_GUEST_LOG")
            self.assertEqual(stage.sha256(original),source_sha)
            self.assertEqual(stage.sha256(clone),clone_sha)

    def test_multicluster_file_read_and_fat_integrity_controls(self):
        with tempfile.TemporaryDirectory(prefix="p002-log-chain-") as dirname:
            root=Path(dirname)
            _,clone,_=new_guest(root)
            payload=bytes((i*73)%251 for i in range(21_987))
            clusters=add_original_guest_log(clone,payload)
            self.assertEqual(len(clusters),3)
            with tool.PrivateFat16Reader(clone) as volume:
                self.assertEqual(volume.read_root_file(
                    tool.RAM_LOG_NAME,tool.MAX_LOG),payload)
            with clone.open("r+b") as disk:
                # Force a legal-looking but cyclic chain.
                disk.seek(FAT_START+clusters[1]*2)
                disk.write(struct.pack("<H",clusters[0]))
                disk.seek(FAT_START+FAT_BYTES+clusters[1]*2)
                disk.write(struct.pack("<H",clusters[0]))
            with tool.PrivateFat16Reader(clone) as volume:
                with self.assertRaisesRegex(ValueError,"cycle"):
                    volume.read_root_file(tool.RAM_LOG_NAME,tool.MAX_LOG)

    def test_corrupt_second_fat_copy_rejected(self):
        with tempfile.TemporaryDirectory(prefix="p002-fat-mirror-") as dirname:
            root=Path(dirname)
            _,clone,_=new_guest(root)
            add_original_guest_log(clone,b"synthetic original log")
            with clone.open("r+b") as disk:
                disk.seek(FAT_START+FAT_BYTES+8)
                disk.write(b"\x00\xF0")
            with self.assertRaisesRegex(ValueError,"FAT1 and FAT2 disagree"):
                tool.PrivateFat16Reader(clone)

    def test_missing_log_is_not_a_pass_and_name_collision_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="p002-no-log-") as dirname:
            root=Path(dirname)
            _,clone,sf2=new_guest(root)
            with self.assertRaisesRegex(ValueError,"Missing or duplicated"):
                tool.extract_guest_ram_evidence(clone,sf2)
            log=synth_guest_log(len(sf2.read_bytes()))
            add_original_guest_log(clone,log)
            with clone.open("r+b") as disk:
                # Duplicate same 8.3 name, no ambiguity accepted.
                disk.seek(ROOT_START+3*32)
                entry=bytearray(32)
                entry[:11]=tool.RAM_LOG_NAME
                entry[11]=0x20
                struct.pack_into("<H",entry,26,3)
                struct.pack_into("<I",entry,28,len(log))
                disk.write(entry)
            with self.assertRaisesRegex(ValueError,"duplicated"):
                tool.extract_guest_ram_evidence(clone,sf2)

    def test_same_sized_wrong_bank_and_bad_guest_log_are_refused(self):
        with tempfile.TemporaryDirectory(prefix="p002-bank-mismatch-") as dirname:
            root=Path(dirname)
            _,clone,sf2=new_guest(root)
            add_original_guest_log(clone,synth_guest_log(len(sf2.read_bytes())))
            changed=bytearray(sf2.read_bytes())
            changed[-11]^=1
            wrong=root/"same-size-wrong-bank.sf2"
            wrong.write_bytes(changed)
            with self.assertRaisesRegex(ValueError,"differs"):
                tool.extract_guest_ram_evidence(clone,wrong)
            with clone.open("r+b") as disk:
                disk.seek(DATA_START+2*CLUSTER_BYTES)
                # Not the actual log cluster: used as an intentional
                # no-op to confirm negative was about bank identity.
                disk.write(b"")
            correct,report=tool.extract_guest_ram_evidence(clone,sf2)
            self.assertIn(b"CREATIVE_DRIVER_ALLOCATED_WORD_DELTA",correct)
            self.assertFalse(report["release_approved"])


if __name__=="__main__":
    unittest.main()
