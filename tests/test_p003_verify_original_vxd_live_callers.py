"""P003 synthetic x86 guest V4 call-edge proof. No Creative binary present.

The authentically loaded Windows95 LCOD base MUST be computed from
86Box PC_after_IO, i.e. opcode-last-byte offset + 1. Only observed
CALL+5 return-address E8 rel32 callers pass. Static caller counts do
not substitute for execution evidence.
"""
import io
import importlib.util
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p003_verify_original_vxd_live_callers import (
    OPCODE_ANCHORS,EMU8K_IO_HELPERS,choose_lcod_bias,validate_direct_return,
    diagnose_last_byte_call_hint, UNKNOWN)
from p003_join_original_guest_cpu_chip_clocks import guest_events
from collections import Counter

class GuestOriginalX86ReturnEdgeTests(unittest.TestCase):
    def setUp(self):
        self.code=bytearray(b"\x90"*31200)
        self.bias=0xa10c5000
        self.call=0x4f92
        self.target=EMU8K_IO_HELPERS["out_dword"]
        self.code[self.call]=0xe8
        struct.pack_into("<i",self.code,self.call+1,
                         self.target-(self.call+5))
    def test_validated_original_return_eip_reaches_right_helper(self):
        # C1018F12 (out-dword index opcode) and C1018F23
        # lead to original 0x3D1B out-dword helper.
        edge=validate_direct_return(bytes(self.code),self.bias,
                                    (self.bias+0x3d2d+1)&0xffffffff,
                                    (self.bias+self.call+5)&0xffffffff)
        self.assertEqual(edge,("out_dword",self.call))
    def test_last_byte_return_hint_is_candidate_not_direct_live_proof(self):
        physical=(self.bias+0x3d2d+1)&0xffffffff
        last_call_byte=(self.bias+self.call+4)&0xffffffff
        self.assertIsNone(validate_direct_return(
            bytes(self.code),self.bias,physical,last_call_byte))
        self.assertEqual(diagnose_last_byte_call_hint(
            bytes(self.code),self.bias,physical,last_call_byte),
            ("out_dword",self.call))
    def test_last_byte_candidate_cannot_alias_wrong_io_helper(self):
        self.assertIsNone(diagnose_last_byte_call_hint(
            bytes(self.code),self.bias,self.bias+0x3cc6+1,
            self.bias+self.call+4))
    def test_last_byte_candidate_cannot_alias_real_e8_return(self):
        # A genuine next-PC return address does NOT match the one-byte
        # discrepancy method and must remain strictly verified normally.
        self.assertIsNone(diagnose_last_byte_call_hint(
            bytes(self.code),self.bias,self.bias+0x3d2d+1,
            self.bias+self.call+5))
    def test_wrong_helper_not_accepted(self):
        # Same original call site but an OUTW physical opcode. One
        # cannot claim this is a direct call into the word helper.
        edge=validate_direct_return(bytes(self.code),self.bias,
                                    (self.bias+0x3cc6+1)&0xffffffff,
                                    (self.bias+self.call+5)&0xffffffff)
        self.assertIsNone(edge)
    def test_return_address_not_call_or_out_of_object_rejected(self):
        self.assertIsNone(validate_direct_return(
            bytes(self.code),self.bias,self.bias+0x3d2d+1,
            self.bias+self.call+4))
        self.assertIsNone(validate_direct_return(
            bytes(self.code),self.bias,self.bias+0x3d2d+1,UNKNOWN))
        self.assertIsNone(validate_direct_return(
            bytes(self.code),self.bias,self.bias+0x3d2d+1,
            self.bias+0xffff))
    def test_exact_bias_is_derived_not_prior_session_constant(self):
        eips=Counter({(self.bias+off+1)&0xffffffff:1000
                      for off in OPCODE_ANCHORS})
        eips[0x61234567]=11 # 11 owner startup I/O EIPs
        bias,support,runner=choose_lcod_bias(eips)
        self.assertEqual(bias,self.bias)
        self.assertEqual(support,12000)
        self.assertNotEqual(bias,(self.bias+1)&0xffffffff)
        self.assertLess(runner,support)
    def test_incomplete_opcode_set_does_not_fake_session_proof(self):
        # A single observed EIP can fit any of the 12 anchors.
        with self.assertRaisesRegex(ValueError,"Ambiguous"):
            choose_lcod_bias(Counter({0xa1003cc6:1000}))
    def test_pre_io_pc_does_not_forge_original_helper_call(self):
        self.assertIsNone(validate_direct_return(
            bytes(self.code),self.bias,
            (self.bias+0x3d2d)&0xffffffff,
            (self.bias+self.call+5)&0xffffffff))
    def test_original_post_io_return_has_no_phantom_call4_offset(self):
        physical=(self.bias+0x3d2d+1)&0xffffffff
        legitimate_return=(self.bias+self.call+5)&0xffffffff
        self.assertEqual(validate_direct_return(
            bytes(self.code),self.bias,physical,legitimate_return),
            ("out_dword",self.call))
        self.assertIsNone(diagnose_last_byte_call_hint(
            bytes(self.code),self.bias,physical,legitimate_return))
    def test_v3_guest_trace_no_callstack(self):
        trace=io.StringIO(
            "# P003_CPU_GUEST_PHYSICAL_8_16_32_V3 synthetic\n"
            "W 1000 0 1111 AAAAAAAA E22 0000 0\n")
        events=list(guest_events(trace))
        self.assertEqual(len(events),1)
        self.assertEqual(len(events[0][-1]),0)
    def test_v4_missing_callstack_rejected(self):
        trace=io.StringIO(
            "# P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK synthetic\n"
            "W 1000 0 1111 AAAAAAAA E22 0000 0\n")
        with self.assertRaisesRegex(ValueError,"wrong width"):
            list(guest_events(trace))

if __name__=="__main__":
    unittest.main()
