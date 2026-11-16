"""Synthetic-only regression for private P003 original V4 mode hints.

Original Win95 guest, SBAWE.VXD and ROM/SF2/audio never enter public CI.
"""
from collections import Counter
import sys
from pathlib import Path
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p003_audit_original_v4_allocator_modes import (
    LCOD_REGIONS, classify_original_hints, audit)

class OriginalV4AllocatorModeEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.bias=0xc10151e5
    def test_original_mode0_and_dispatcher_distinct_stack_levels(self):
        frames=[
            Counter({self.bias+0x3f12:7,self.bias+0x3fcd:3}),
            Counter({self.bias+0x3da1:10}),
            Counter(), Counter(),
        ]
        r=classify_original_hints(self.bias,frames)
        self.assertEqual(
            r["mode_0_three_lane"]["physical_port_events_with_return_hints_in_code_region"],10)
        self.assertEqual(
            r["mode_0_three_lane"]["return_hint_depth_counts"],{0:10})
        self.assertEqual(
            r["dispatcher"]["return_hint_depth_counts"],{1:10})
        for k in ("mode_1_adjacent_pair","mode_2_packed_range"):
            self.assertEqual(
                r[k]["physical_port_events_with_return_hints_in_code_region"],0)
            self.assertIn("HINT_ONLY",r[k]["function_execution_status"])
    def test_pair_and_range_classification_are_separate(self):
        frames=[Counter({self.bias+0x3ff0:5,self.bias+0x4180:7}),
                Counter(),Counter(),Counter()]
        r=classify_original_hints(self.bias,frames)
        self.assertEqual(
            r["mode_1_adjacent_pair"]["physical_port_events_with_return_hints_in_code_region"],5)
        self.assertEqual(
            r["mode_2_packed_range"]["physical_port_events_with_return_hints_in_code_region"],7)
        self.assertEqual(
            r["mode_0_three_lane"]["physical_port_events_with_return_hints_in_code_region"],0)
    def test_one_byte_unknown_region_boundary_not_silently_corrected(self):
        low, high=LCOD_REGIONS["mode_1_adjacent_pair"]
        frames=[Counter({self.bias+low:1,self.bias+low+1:2,
                         self.bias+high-1:1,self.bias+high:1}),
                Counter(),Counter(),Counter()]
        mode=classify_original_hints(self.bias,frames)["mode_1_adjacent_pair"]
        self.assertEqual(mode["physical_port_events_with_return_hints_in_code_region"],2)
        self.assertEqual(mode["distinct_return_hint_offsets"],1)
    def test_unavailable_address_does_not_forge_a_voice_mode(self):
        frames=[Counter({0xffffffff:100}),Counter(),Counter(),Counter()]
        r=classify_original_hints(self.bias,frames)
        self.assertEqual(sum(x["physical_port_events_with_return_hints_in_code_region"]
                             for x in r.values()),0)
    def test_missing_stack_level_fails(self):
        with self.assertRaisesRegex(ValueError,"Four raw V4"):
            classify_original_hints(self.bias,[Counter(),Counter()])
    def test_negative_or_zero_hint_count_fails_closed(self):
        with self.assertRaisesRegex(ValueError,"Invalid original"):
            classify_original_hints(self.bias,[Counter({self.bias+0x3f12:-1}),
                                              Counter(),Counter(),Counter()])
    def test_no_private_vxd_is_rejected_before_zip(self):
        with self.assertRaisesRegex(ValueError,"SHA256 differs"):
            audit(b"NOT THE ORIGINAL CREATIVE VXD",
                  Path("THIS-ZIP-IS-NOT-CHECKED-BECAUSE-SHA-FAILS.zip"))

if __name__=="__main__":
    unittest.main()
