"""P003 synthetic negative controls for the SHA-locked VXD static caller audit.

No proprietary VXD image, Creative ROM/SF2, or owner CPU trace is included.
"""
import importlib.util
from pathlib import Path
import struct
import unittest

path=Path(__file__).resolve().parents[1] / "tools" / "p003_private_vxd_static_callers.py"
spec=importlib.util.spec_from_file_location("p003_private_vxd_static_callers",path)
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class OriginalVxdCallerStaticResearch(unittest.TestCase):
    def test_direct_relative_call_targets(self):
        buf=bytearray(b"\x90"*128)
        buf[4:7]=b"\x55\x8b\xec"
        # 32-bit relative call at 40 targets a synthetic helper at 96.
        buf[40]=0xe8
        struct.pack_into("<i",buf,41,96-(40+5))
        buf[56]=0xe8
        struct.pack_into("<i",buf,57,97-(56+5))
        buf[70]=0xe8
        struct.pack_into("<i",buf,71,96-(70+5))
        result=module.direct_callers(bytes(buf),{"helper":96})
        self.assertEqual(result,{"helper":[40,70]})
        self.assertEqual(module.nearest_prologue_hint(bytes(buf),40),4)
        self.assertEqual(module.nearest_prologue_hint(bytes(buf),70),4)
    def test_reject_duplicate_target_addresses(self):
        with self.assertRaisesRegex(ValueError,"Duplicate"):
            module.direct_callers(b"\x90"*50,{"first":10,"second":10})
    def test_bytes_inside_data_are_not_incorrectly_certified(self):
        b=bytearray(b"\x00"*40)
        b[4]=0xe8
        struct.pack_into("<i",b,5,20-(4+5))
        # This is a raw E8 byte sequence. The audit labels it a
        # CANDIDATE; it cannot prove an instruction boundary or
        # that this branch executed in the owner Windows95 trace.
        self.assertEqual(module.direct_callers(b,{"h":20})["h"],[4])
    def test_original_helper_anchors_pinned(self):
        self.assertEqual(len(module.OPCODE_ANCHORS),12)
        self.assertEqual(len(module.EMU8K_IO_HELPERS),6)
        self.assertNotEqual(module.PINNED_VXD_SHA256,"")
    def test_wrong_private_driver_fails_before_parsing(self):
        with self.assertRaisesRegex(ValueError,"SHA256 differs"):
            module.original_lcod(b"NOT THE CREATIVE VXD")
    def test_truncated_file_does_not_bypass_sha(self):
        with self.assertRaisesRegex(ValueError,"SHA256 differs"):
            module.original_lcod(b"MZ" + b"\x00"*20)
    def test_prologue_hint_is_only_heuristic(self):
        self.assertIsNone(module.nearest_prologue_hint(b"\x90"*100,95))

if __name__=="__main__":
    unittest.main()
