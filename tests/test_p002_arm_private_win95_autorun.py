"""Synthetic-only test of disposable Win95 WIN.INI run= autorun staging.

No Microsoft Win95 files, Creative drivers, ROM or personal SF2 present.
Uses an artificially generated sparse FAT16 image and dummy MZ file.
This does not start 86Box or certify any original driver waveform.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct
import tempfile
import unittest

from tools import p002_arm_private_win95_autorun as autorun
from tools import p002_extract_original_win95_ram_log_fat16 as fat16
from test_p002_win95_ram_stage import write_inputs
from test_p002_extract_win95_ram_log_fat16 import (
    ROOT_START, CLUSTER_BYTES)
from test_p002_attest_private_win95_creative_driver_fat16 import put_fat_file


def fixture(private: Path, *, existing_run=False, multiple_cluster=False,
            missing_winini=False):
    source,exe,bank=write_inputs(private)
    with source.open("r+b") as image:
        image.seek(458)
        image.write(struct.pack("<I",1_048_000))
    win_dir=put_fat_file(source,ROOT_START,512,
                         autorun.WINDOWS_DIR,
                         b"\0"*CLUSTER_BYTES,is_directory=True)
    if not missing_winini:
        prefix=b"[windows]\r\nload=\r\n"
        if existing_run:
            prefix+=b"run=C:\\DO-NOT-OVERWRITE.EXE\r\n"
        else:
            prefix+=b"run=\r\n"
        payload=prefix+b"NullPort=None\r\n\r\n[Desktop]\r\nWallpaper=(None)\r\n"
        if multiple_cluster:
            payload+=b"x"*(CLUSTER_BYTES+16)
        put_fat_file(source,win_dir,CLUSTER_BYTES//32,
                     autorun.WIN_INI,payload)
    return source,exe,bank


class OriginalWin95DisposableAutostartTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix="p002-win95-run-")
        self.root=Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def test_existing_guest_exe_automatically_runs_only_in_new_clone(self):
        original,exe,sf2=fixture(self.root)
        oldsha=autorun.sha256(original)
        clone=self.root/"P002-PRIVATE-ONLY.img"
        report=autorun.prepare(original,clone,exe,sf2)
        self.assertEqual(report["status"],
                         "PRIVATE_WIN95_CLONE_WIN_INI_AUTORUN_ARMED")
        self.assertEqual(report["guest_autorun"],"C:\\P002RAM.EXE")
        self.assertFalse(report["guest_creative_driver_executed"])
        self.assertFalse(report["86Box_guest_booted"])
        self.assertFalse(report["release_approved"])
        self.assertEqual(autorun.sha256(original),oldsha)
        self.assertNotEqual(autorun.sha256(clone),oldsha)
        with fat16.PrivateFat16Reader(original) as reader:
            old=reader.read_short_path_file(
                (autorun.WINDOWS_DIR,autorun.WIN_INI),autorun.MAX_WIN_INI)
            self.assertIn(b"\r\nrun=\r\n",old)
        with fat16.PrivateFat16Reader(clone) as reader:
            new=reader.read_short_path_file(
                (autorun.WINDOWS_DIR,autorun.WIN_INI),autorun.MAX_WIN_INI)
            self.assertIn(b"\r\nrun=C:\\P002RAM.EXE\r\n",new)
            self.assertEqual(new.replace(autorun.RUN_VALUE,b"run="),old)
            self.assertEqual(reader.read_root_file(
                fat16.RAM_EXE_NAME,1_000_000),exe.read_bytes())
            self.assertEqual(reader.read_root_file(
                fat16.RAM_SF2_NAME,fat16.MAX_BANK),sf2.read_bytes())
            self.assertEqual(reader.fat[:4096],reader.fat[:4096])
        with self.assertRaisesRegex(ValueError,"New disposable Win95 clone required"):
            autorun.prepare(original,clone,exe,sf2)

    def test_occupied_original_winini_run_is_never_overwritten(self):
        original,exe,sf2=fixture(self.root,existing_run=True)
        oldsha=autorun.sha256(original)
        clone=self.root/"must-not-survive-occupied-run.img"
        with self.assertRaisesRegex(ValueError,"already occupied"):
            autorun.prepare(original,clone,exe,sf2)
        self.assertFalse(clone.exists())
        self.assertEqual(autorun.sha256(original),oldsha)

    def test_original_winini_multicluster_refused_without_edit(self):
        original,exe,sf2=fixture(self.root,multiple_cluster=True)
        oldsha=autorun.sha256(original)
        clone=self.root/"must-not-survive-multicluster.img"
        with self.assertRaisesRegex(ValueError,"single FAT16 cluster"):
            autorun.prepare(original,clone,exe,sf2)
        self.assertFalse(clone.exists())
        self.assertEqual(autorun.sha256(original),oldsha)

    def test_missing_original_winini_is_not_reinvented(self):
        original,exe,sf2=fixture(self.root,missing_winini=True)
        clone=self.root/"must-not-survive-missing-ini.img"
        with self.assertRaisesRegex(ValueError,"Win95 FAT16 directory missing or duplicate"):
            autorun.prepare(original,clone,exe,sf2)
        self.assertFalse(clone.exists())

    def test_direct_patch_cannot_modify_original_and_requires_staged_inputs(self):
        original,exe,sf2=fixture(self.root)
        source_sha=autorun.sha256(original)
        with self.assertRaisesRegex(ValueError,"separately staged"):
            autorun.arm_disposable_clone(original,original,exe,sf2)
        fake=self.root/"fake-staged-disk.img"
        fake.write_bytes(b"not an original Windows image")
        with self.assertRaisesRegex(ValueError,"512 MiB"):
            autorun.arm_disposable_clone(fake,original,exe,sf2)
        self.assertEqual(autorun.sha256(original),source_sha)


if __name__=="__main__":
    unittest.main()
