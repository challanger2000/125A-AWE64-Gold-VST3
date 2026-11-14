"""Original corrected Win95 source-session SHA identity regressions.

Only synthetic temporary bytes are used; these never attest a Win95 boot
or clone the owner's Creative binaries.
"""
import json
import hashlib
from pathlib import Path
import tempfile
import unittest
import wave
from unittest.mock import patch

from tools import p002_attest_private_win95_session as source

class SourceManifestTests(unittest.TestCase):
    def setUp(self):
        tmp=tempfile.TemporaryDirectory(prefix="p002-real-guest-session-contract-")
        self.addCleanup(tmp.cleanup)
        self.root=Path(tmp.name)
        self.files={
            name:self.root/name
            for name in ("original.trace","source.sf2","guest.log","original.wav")
        }
        for i,p in enumerate(self.files.values()):
            p.write_bytes(("Synthetic owner session "+str(i)).encode("ascii"))
        self.manifest=self.root/"private-original-session.json"
        self.original_sha=source.sha256_file

    def _manifest(self):
        return {
            "schema":source.SCHEMA,
            "original_emu8k_trace_sha256":
                self.original_sha(self.files["original.trace"]),
            "original_guest_log_sha256":
                self.original_sha(self.files["guest.log"]),
            "original_win95_precard_wav_sha256":
                self.original_sha(self.files["original.wav"]),
            "original_owner_sf2_sha256":source.SF2_SHA,
            "original_private_booted_disk_sha256":"a"*64,
            "corrected_win95_p002ram_exe_sha256":source.CORRECTED_GUEST_EXE_SHA,
            "installed_original_sbawe_vxd_sha256":source.VXD_SHA,
            "installed_original_aweman32_dll_sha256":source.DLL_SHA,
            "original_rom_sha256":source.ROM_SHA,
            "calibrated_original_fork_exe_sha256":
                source.CALIBRATED_FORK_EXE_SHA,
            "original_source_sf2_ram_words_bit_equal":260921,
            "original_source_ram_rewrites":255,
            "guest_log_from_same_private_fat16_disk":True,
            "original_trace_and_wav_same_86box_process":
                "MUST_BE_CAPTURED_TOGETHER_BY_START_P002",
            "original_win95_vs_native_vst3_pcm_parity":"NOT_COMPARED",
            "release_approved":False,
        }

    def verify(self,info):
        self.manifest.write_text(json.dumps(info),encoding="utf-8")
        def pinned_bank(path):
            return source.SF2_SHA if path==self.files["source.sf2"] else self.original_sha(path)
        with patch.object(source,"sha256_file",side_effect=pinned_bank):
            return source.verify_manifest(
                self.manifest,self.files["original.trace"],
                self.files["source.sf2"],self.files["guest.log"],
                self.files["original.wav"])

    def test_real_session_dynamic_log_sha_not_fabricated_fixed_hash(self):
        manifest=self._manifest()
        result=self.verify(manifest)
        self.assertEqual(result["status"],
                         "ORIGINAL_WIN95_REAL_SESSION_SOURCE_HASHES_VALID")
        self.assertFalse(result["release_approved"])
        self.assertEqual(result["original_guest_log_sha256"],
                         self.original_sha(self.files["guest.log"]))
        self.assertNotEqual(result["original_guest_log_sha256"],
                            "660a01264550415d50d2cd071e752e9388ebb63071288acbefe95c0386a07096")

    def test_different_actual_guest_log_and_trace_wav_no_go(self):
        manifest=self._manifest()
        self.verify(manifest)
        for changed in ("guest.log","original.trace","original.wav"):
            path=self.files[changed]
            before=path.read_bytes()
            path.write_bytes(before+b"\n")
            with self.subTest(changed=changed):
                with self.assertRaisesRegex(ValueError,
                         "original_(guest_log|emu8k_trace|win95_precard_wav)_sha256"):
                    self.verify(manifest)
            path.write_bytes(before)

    def test_forged_source_driver_boot_and_claimed_parity_no_go(self):
        valid=self._manifest()
        for key,value in (
            ("corrected_win95_p002ram_exe_sha256","f"*64),
            ("installed_original_sbawe_vxd_sha256","f"*64),
            ("installed_original_aweman32_dll_sha256","f"*64),
            ("original_rom_sha256","f"*64),
            ("calibrated_original_fork_exe_sha256","f"*64),
            ("original_private_booted_disk_sha256","not-a-hash"),
            ("original_source_sf2_ram_words_bit_equal",0),
            ("original_source_ram_rewrites",254),
            ("guest_log_from_same_private_fat16_disk",False),
            ("original_win95_vs_native_vst3_pcm_parity","PASS"),
            ("release_approved",True),
        ):
            with self.subTest(key=key):
                with self.assertRaises(ValueError):
                    self.verify(valid|{key:value})

    def test_no_manifest_or_non_json_no_go(self):
        with self.assertRaisesRegex(ValueError,"missing"):
            source.verify_manifest(self.manifest,*self.files.values())
        self.manifest.write_bytes(b'{"schema"')
        with self.assertRaisesRegex(ValueError,"Invalid"):
            source.verify_manifest(self.manifest,*self.files.values())

if __name__=="__main__":
    unittest.main()
