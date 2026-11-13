"""Synthetic-only offline original Win95 Creative driver FAT16 attestation.

All 512-MiB images are GENERATED sparse dummy FAT16 test volumes.
No Windows 95 OS, authentic Creative binary, ROM, SF2 or user data here.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct
import tempfile
import unittest

from tools import p002_attest_private_win95_creative_driver_fat16 as audit
from tools import p002_extract_original_win95_ram_log_fat16 as fat16
from test_p002_extract_win95_ram_log_fat16 import (
    new_guest, FAT_START, FAT_BYTES, ROOT_START, DATA_START, CLUSTER_BYTES)

ROOT_ENTRIES=512


def put_fat_file(image: Path, parent_offset: int, parent_entries: int,
                 name: bytes, payload: bytes, *, is_directory=False) -> int:
    assert len(name) == 11 and payload
    size = len(payload)
    num = (size + CLUSTER_BYTES - 1) // CLUSTER_BYTES
    with image.open("r+b") as disk:
        disk.seek(FAT_START)
        fat = disk.read(FAT_BYTES)
        free = [c for c in range(2, 65000)
                if struct.unpack_from("<H", fat, c*2)[0] == 0][:num]
        assert len(free) == num
        for index, cluster in enumerate(free):
            following = 0xFFFF if index+1 == num else free[index+1]
            for mirror in (0, 1):
                disk.seek(FAT_START + mirror*FAT_BYTES + cluster*2)
                disk.write(struct.pack("<H", following))
            disk.seek(DATA_START + (cluster-2)*CLUSTER_BYTES)
            disk.write(payload[index*CLUSTER_BYTES:
                               (index+1)*CLUSTER_BYTES].ljust(
                CLUSTER_BYTES, b"\0"))
        disk.seek(parent_offset)
        directory = disk.read(parent_entries*32)
        free_slot = next((i for i in range(0,len(directory),32)
                          if directory[i] in (0,0xE5)),None)
        assert free_slot is not None
        entry = bytearray(32)
        entry[:11] = name
        entry[11] = 0x10 if is_directory else 0x20
        struct.pack_into("<H",entry,26,free[0])
        struct.pack_into("<I",entry,28,0 if is_directory else size)
        disk.seek(parent_offset+free_slot)
        disk.write(entry)
    return DATA_START+(free[0]-2)*CLUSTER_BYTES


def make_private_synthetic_image(base: Path):
    _, image, _ = new_guest(base)
    win=put_fat_file(image,ROOT_START,ROOT_ENTRIES,audit.WINDOWS,
                     b"\0"*CLUSTER_BYTES,is_directory=True)
    sys=put_fat_file(image,win,CLUSTER_BYTES//32,audit.SYSTEM,
                     b"\0"*CLUSTER_BYTES,is_directory=True)
    vendor=put_fat_file(image,ROOT_START,ROOT_ENTRIES,audit.AWE32,
                        b"\0"*CLUSTER_BYTES,is_directory=True)
    sf2_folder=put_fat_file(image,ROOT_START,ROOT_ENTRIES,audit.SF2,
                            b"\0"*CLUSTER_BYTES,is_directory=True)
    # Multicluster original-looking-but-SYNTHETIC driver bytes:
    # exercise true FAT16 chain traversal and exact SHA256 over content.
    dll=b"MZ"+bytes((i*31)%251 for i in range(17001))
    vxd=b"LE"+bytes((i*17)%251 for i in range(20133))
    bank_payload=bytes((i*3)%251 for i in range(18999))
    bank=b"RIFF"+struct.pack("<I",len(bank_payload)+4)+b"sfbk"+bank_payload
    locations={}
    locations["dll_windows"]=put_fat_file(
        image,win,CLUSTER_BYTES//32,audit.AWEMAN32,dll)
    locations["dll_system"]=put_fat_file(
        image,sys,CLUSTER_BYTES//32,audit.AWEMAN32,dll)
    locations["vxd_system"]=put_fat_file(
        image,sys,CLUSTER_BYTES//32,audit.SBAWE,vxd)
    locations["vxd_vendor"]=put_fat_file(
        image,vendor,CLUSTER_BYTES//32,audit.SBAWE,vxd)
    locations["sf2"]=put_fat_file(
        image,sf2_folder,CLUSTER_BYTES//32,audit.SYNTH512,bank)
    source=base/"synthetic-owner-source-synth512.sf2"
    source.write_bytes(bank)
    return image,source,dll,vxd,locations


class PrivateCreativeDriverFAT16Tests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix="p002-creative-fat-")
        self.base=Path(self.temp.name)
        (self.image,self.bank,self.dll,self.vxd,
         self.locations)=make_private_synthetic_image(self.base)
        self.expected_dll=hashlib.sha256(self.dll).hexdigest()
        self.expected_vxd=hashlib.sha256(self.vxd).hexdigest()

    def tearDown(self):
        self.temp.cleanup()

    def attest(self, source=True):
        return audit.attest(
            self.image,self.expected_vxd,self.expected_dll,
            self.bank if source else None)

    def test_exact_multicluster_read_only_file_hash_and_sf2(self):
        original_sha=fat16.sha256_file(self.image)
        report=self.attest()
        self.assertEqual(report["status"],
                         "P002_PRIVATE_WIN95_INSTALLED_CREATIVE_FILE_SHA256_MATCH")
        self.assertEqual(report["aweman32_sha256"],self.expected_dll)
        self.assertEqual(report["sbawe_vxd_sha256"],self.expected_vxd)
        self.assertTrue(report["guest_windows_and_system_aweman_identical"])
        self.assertTrue(report["guest_system_and_vendor_sbawe_identical"])
        self.assertTrue(report["guest_staged_synth512_source_bank_identical"])
        self.assertEqual(report["guest_disk_sha256"],original_sha)
        self.assertEqual(fat16.sha256_file(self.image),original_sha)
        self.assertEqual(report["actual_win95_Creative_dll_vxd_execution"],
                         "NOT_VERIFIED_BY_OFFLINE_FILE_ATTESTATION")
        self.assertFalse(report["release_approved"])
        with fat16.PrivateFat16Reader(self.image) as disk:
            self.assertEqual(disk.read_short_path_file(
                (audit.WINDOWS,audit.SYSTEM,audit.SBAWE),audit.MAX_VXD),
                self.vxd)
            self.assertEqual(disk.read_short_path_file(
                (audit.WINDOWS,audit.AWEMAN32),audit.MAX_DLL),self.dll)
            with self.assertRaisesRegex(ValueError,"missing/ambiguous"):
                disk.read_short_path_file((b"WRONG       ",audit.SBAWE),
                                          audit.MAX_VXD)
            with self.assertRaisesRegex(ValueError,"path component"):
                disk.read_short_path_file((b"WINDOWS",audit.AWEMAN32),
                                          audit.MAX_DLL)
            with self.assertRaisesRegex(ValueError,"oversized"):
                disk.read_short_path_file(
                    (audit.WINDOWS,audit.AWEMAN32),32)

    def test_optional_bank_omission_never_claims_audio(self):
        report=self.attest(source=False)
        self.assertEqual(report["guest_staged_synth512_source_bank_identical"],
                         "NOT_CHECKED_NO_OWNER_SOURCE_SUPPLIED")
        self.assertEqual(report["original_EMU8000_ram_sf2_pcm"],
                         "NOT_VERIFIED_BY_OFFLINE_FILE_ATTESTATION")

    def test_wrong_external_expected_driver_sha_rejected(self):
        wrong="0"*64
        with self.assertRaisesRegex(ValueError,"SBAWE.VXD SHA256"):
            audit.attest(self.image,wrong,self.expected_dll,self.bank)
        with self.assertRaisesRegex(ValueError,"AWEMAN32.DLL SHA256"):
            audit.attest(self.image,self.expected_vxd,wrong,self.bank)
        with self.assertRaisesRegex(ValueError,"64 hex digits"):
            audit.attest(self.image,"not-a-hash",self.expected_dll,self.bank)

    def flip(self,key):
        with self.image.open("r+b") as disk:
            pos=self.locations[key]+53
            disk.seek(pos);before=disk.read(1)
            disk.seek(pos);disk.write(bytes([before[0]^1]))

    def test_shadow_dll_and_vendor_vxd_changes_refused(self):
        self.flip("dll_windows")
        with self.assertRaisesRegex(ValueError,"shadow copy differs"):
            self.attest()
        # Separate new pristine disk for independent VXD source mutation.
        self.temp.cleanup()
        self.temp=tempfile.TemporaryDirectory(prefix="p002-creative-fat-")
        self.base=Path(self.temp.name)
        self.image,self.bank,self.dll,self.vxd,self.locations=(
            make_private_synthetic_image(self.base))
        self.expected_dll=hashlib.sha256(self.dll).hexdigest()
        self.expected_vxd=hashlib.sha256(self.vxd).hexdigest()
        self.flip("vxd_vendor")
        with self.assertRaisesRegex(ValueError,"install copy"):
            self.attest()

    def test_host_sf2_identity_and_mirrored_fat_refused(self):
        changed=bytearray(self.bank.read_bytes());changed[-1]^=1
        wrong=self.base/"synthetic-wrong-owner-bank.sf2"
        wrong.write_bytes(changed)
        with self.assertRaisesRegex(ValueError,"differs"):
            audit.attest(self.image,self.expected_vxd,
                         self.expected_dll,wrong)
        with self.image.open("r+b") as disk:
            disk.seek(FAT_START+FAT_BYTES+200)
            disk.write(b"\xEE")
        with self.assertRaisesRegex(ValueError,"FAT1 and FAT2 disagree"):
            self.attest()


if __name__ == "__main__":
    unittest.main()
