"""P120 public synthetic oneclick environment and private archive safety QA."""
from __future__ import annotations
from pathlib import Path
import json
import sys
import tempfile
import unittest
import zipfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import p120_oneclick_owner_capture as m

class P120OneclickTests(unittest.TestCase):
    def test_env_exact_no_unscoped_capture(self):
        with tempfile.TemporaryDirectory() as td:
            e=m.configure_traces(Path(td).resolve())
            self.assertEqual(len(e),19)
            self.assertEqual(e["AWE64_P107_LCOD_BASE"],"0xC10151E4")
            self.assertEqual(e["AWE64_P110_LCOD_SELECTOR"],"0x28")
            self.assertTrue(e["AWE64_P119_PENDING_JIT_CYCLES_TRACE"].endswith(
                m.TRACE_NAMES['p119']))
            self.assertEqual(len(set(e[k] for k in e if k.endswith("_TRACE"))),9)
            for v in (e[k] for k in e if k.endswith("_TRACE")):
                self.assertTrue(Path(v).is_absolute())
                self.assertIn(str(Path(td).resolve()),v)
            self.assertNotIn("EMU8K_TRACE",e)
            self.assertNotIn("AWE32_WAV",e)
    def test_reject_relative_trace_target(self):
        with self.assertRaisesRegex(ValueError,'absolute'):
            m.configure_traces(Path("relative/trace"))
    def test_private_zip_whitelisted_and_no_rom_sf2_vxd(self):
        with tempfile.TemporaryDirectory() as td:
            z=Path(td);source=z/"old-p003.zip";target=z/"new-p120.zip"
            traces=z/"local-private";traces.mkdir()
            (traces/m.TRACE_NAMES['p119']).write_bytes(
                b'# P119_ORIGINAL_X64_JIT_PENDING_CYCLES_V1 fake\nY 0 1 C101920B 1 2 -3 0 1\n')
            with zipfile.ZipFile(source,'w') as w:
                w.writestr('Original-CPU-IO.trace',b'# original physical IO\nW 100 0 E22\n')
                w.writestr('Original-DRY.wav',b'RIFFAAAAWAVEfake')
                w.writestr('Original-Bank-1.sf2',b'SENSITIVE_PRIVATE_SOUNDFONT')
                w.writestr('Creative-SBAWE.VXD',b'SENSITIVE_PRIVATE_DRIVER')
                w.writestr('Win95-Original.img',b'SENSITIVE_PRIVATE_WINDOWS')
                w.writestr('awe32.raw',b'SENSITIVE_PRIVATE_ROM')
            info={"status":"NO_GO","error":"test synthetic private source","original_parity":False}
            m.stage_private_zipped_result(target,traces,source,info)
            with zipfile.ZipFile(target) as zf:
                names=zf.namelist()
                self.assertIn("P120-ORIGINAL-STATUS.json",names)
                self.assertIn("ORIGINAL-FORSCHUNG/P119-ORIGINAL-UNFLUSHED-JIT-CYCLES.trace",names)
                self.assertIn("ORIGINAL-FORSCHUNG/P003-ORIGINAL-CPU-IO.trace",names)
                self.assertIn("ORIGINAL-FORSCHUNG/P003-ORIGINAL-DRY-PCM.wav",names)
                self.assertNotIn("Original-Bank-1.sf2",names)
                self.assertNotIn("Creative-SBAWE.VXD",names)
                self.assertNotIn("Win95-Original.img",names)
                self.assertNotIn("awe32.raw",names)
                self.assertNotIn(b'SENSITIVE_PRIVATE_ROM',zf.read("START-HIER.txt"))
                report=json.loads(zf.read("P120-ORIGINAL-STATUS.json"))
                self.assertEqual(report['status'],'NO_GO')
    def test_p003_archive_member_cap_fails_closed(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td);src=root/'old.zip';dst=root/'result.zip'
            trace=root/'private';trace.mkdir()
            with zipfile.ZipFile(src,'w') as z:
                z.writestr("Original-CPU-IO.trace",b'x'*256)
            original=m.MAX_ZIP_MEMBER
            try:
                m.MAX_ZIP_MEMBER=20
                with self.assertRaisesRegex(ValueError,'oversized'):
                    m.stage_private_zipped_result(dst,trace,src,{"status":"NO_GO"})
            finally:
                m.MAX_ZIP_MEMBER=original
            self.assertFalse(dst.exists())

if __name__=="__main__":
    unittest.main()
