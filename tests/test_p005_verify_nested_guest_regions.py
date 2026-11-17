#!/usr/bin/env python3
"""Synthetic-only negative controls for supplemental nested Win95 capture."""
from __future__ import annotations
import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p005_verify_nested_guest_regions import NestedError,parse_nested

DEVICE=0x1592
ROOT=0xc30c0ff0
PREFIX=76
ROWS=2*26
P=(ROOT+PREFIX,ROOT+PREFIX+ROWS,
   ROOT+PREFIX+ROWS+100,ROOT+PREFIX+ROWS+200,
   ROOT+PREFIX+ROWS+300,ROOT+PREFIX+ROWS+400,
   ROOT+PREFIX+ROWS+500)

def fixtures():
    device=bytearray(DEVICE)
    struct.pack_into("<H",device,6,32)
    struct.pack_into("<I",device,0x158e,0x4d474d31)
    struct.pack_into("<I",device,0x6ca,ROOT)
    first=bytearray(PREFIX+ROWS)
    struct.pack_into("<7I",first,0,*P)
    struct.pack_into("<HBB",first,PREFIX,1,7,0)
    struct.pack_into("<H",first,PREFIX+26,0xffff)
    primary=(b"125AVXD1"+
             struct.pack("<7I",1,0xc30b09f0,0x3000,DEVICE,288,1,1)+
             device+struct.pack("<II",ROOT,len(first))+first)
    extra=bytearray(b"125AVXR1")
    extra+=struct.pack("<4I",1,0xc30b09f0,0x3000,ROOT)
    extra+=struct.pack("<7I",*P)
    extra+=struct.pack("<I",6)
    for i in range(1,7):
        length=P[i+1]-P[i] if i<6 else 4096
        extra+=struct.pack("<3I",P[i],length,int(i<6))
        extra+=bytes([i])*length
    return primary,bytes(extra)

class NestedFileChecks(unittest.TestCase):
    def test_valid_original_shaped_pair(self):
        primary,nested=fixtures()
        result=parse_nested(nested,primary)
        self.assertEqual(result["bounded_additional_regions_captured"],5)
        self.assertEqual(result["last_region_partial_sample_bytes"],4096)
        self.assertFalse(result["vst3_production_authorized"])
        self.assertFalse(result["last_region_complete"])
    def bad(self,mutate):
        primary,nested=fixtures()
        a=bytearray(nested)
        mutate(a)
        with self.assertRaises(NestedError):
            parse_nested(bytes(a),primary)
    def test_wrong_magic(self):
        self.bad(lambda a:a.__setitem__(0,0))
    def test_wrong_version(self):
        self.bad(lambda a:struct.pack_into("<I",a,8,2))
    def test_wrong_root(self):
        self.bad(lambda a:struct.pack_into("<I",a,20,ROOT+4))
    def test_wrong_cr3(self):
        self.bad(lambda a:struct.pack_into("<I",a,16,0x4000))
    def test_corrupt_seventh_pointer(self):
        self.bad(lambda a:struct.pack_into("<I",a,24+6*4,P[5]-1))
    def test_changed_first_pointer(self):
        self.bad(lambda a:struct.pack_into("<I",a,24,P[0]+4))
    def test_wrong_section_count(self):
        self.bad(lambda a:struct.pack_into("<I",a,52,5))
    def test_missing_payload(self):
        self.bad(lambda a:a.pop())
    def test_surplus_bytes(self):
        self.bad(lambda a:a.extend(b"z"))
    def test_fake_completed_last_region(self):
        def m(a):
            pos=56
            for i in range(1,7):
                if i==6:
                    struct.pack_into("<I",a,pos+8,1)
                    return
                pos+=12+(P[i+1]-P[i])
        self.bad(m)
    def test_cross_capture_mismatch(self):
        primary,nested=fixtures()
        altered=bytearray(primary)
        struct.pack_into("<I",altered,36+DEVICE+8+4*3,P[3]+1)
        with self.assertRaises(NestedError):
            parse_nested(nested,bytes(altered))

if __name__=="__main__":
    unittest.main(verbosity=2)
