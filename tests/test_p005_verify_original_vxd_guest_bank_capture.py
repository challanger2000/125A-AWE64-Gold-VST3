#!/usr/bin/env python3
"""P005 capture-file source schema negative controls, SYNTHETIC BYTES ONLY."""
from __future__ import annotations
import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p005_verify_original_vxd_guest_bank_capture import (
    CaptureError,DEVICE_BYTES,ROW_BYTES,parse_capture
)

def fake_capture()->bytes:
    device=bytearray(DEVICE_BYTES)
    struct.pack_into("<I",device,0x158e,0x4d474d31)
    struct.pack_into("<H",device,6,32)
    struct.pack_into("<I",device,0x6ca+5*4,0xc30c0ff0)
    struct.pack_into("<I",device,0x6ca+6*4,0xc30c0ff0)
    row=bytearray(ROW_BYTES*2)
    struct.pack_into("<H",row,0,1)
    row[2]=7
    row[3]=1
    struct.pack_into("<H",row,ROW_BYTES,0xffff)
    return (b"125AVXD1"+
            struct.pack("<7I",1,0xc30b09f0,0x3000,DEVICE_BYTES,288,1,2)+
            device+
            struct.pack("<II",0xc30c0ff0,len(row))+row)

class OriginalGuestCaptureTests(unittest.TestCase):
    def test_genuine_shaped_source_not_auth(self):
        report=parse_capture(fake_capture())
        self.assertEqual(report["original_vxd_program_slots"],288)
        self.assertEqual(report["nonzero_original_slot_pointers"],2)
        self.assertEqual(report["unique_guest_program_record_lists"],1)
        self.assertEqual(report["normalized_source_rows_including_sentinels"],2)
        self.assertFalse(report["production_vst3_activation_authorized"])
        self.assertIn("PROVENANCE",report["status"])

    def bad(self,mutator):
        source=bytearray(fake_capture())
        mutator(source)
        with self.assertRaises(CaptureError):
            parse_capture(bytes(source))
    def test_wrong_magic(self):
        self.bad(lambda x:x.__setitem__(0,0))
    def test_wrong_version(self):
        self.bad(lambda x:struct.pack_into("<I",x,8,2))
    def test_wrong_image_size(self):
        self.bad(lambda x:struct.pack_into("<I",x,20,DEVICE_BYTES-1))
    def test_wrong_original_slots(self):
        self.bad(lambda x:struct.pack_into("<I",x,24,128))
    def test_wrong_pointer_count(self):
        self.bad(lambda x:struct.pack_into("<I",x,32,1))
    def test_unmapped_guest_pointer(self):
        self.bad(lambda x:struct.pack_into("<I",x,36+0x6ca+5*4,0xc30d1000))
    def test_wrong_device_magic(self):
        self.bad(lambda x:struct.pack_into("<I",x,36+0x158e,0))
    def test_wrong_physical_voice_capacity(self):
        self.bad(lambda x:struct.pack_into("<H",x,36+6,16))
    def test_misaligned_guest_cr3(self):
        self.bad(lambda x:struct.pack_into("<I",x,16,0x3001))
    def test_missing_source_sentinel(self):
        self.bad(lambda x:struct.pack_into("<H",x,len(x)-26,0))
    def test_early_source_sentinel(self):
        self.bad(lambda x:struct.pack_into("<H",x,len(x)-52,0xffff))
    def test_extra_trailing_source_bytes(self):
        self.bad(lambda x:x.extend(b"\0"))
    def test_truncated_source_rows(self):
        self.bad(lambda x:x.pop())
    def test_row_length_not_26_multiple(self):
        self.bad(lambda x:struct.pack_into("<I",x,36+DEVICE_BYTES+4,53))
    def test_untrusted_guest_device_address(self):
        self.bad(lambda x:struct.pack_into("<I",x,12,0x4000))
    def test_indirect_bank_pointer_header_must_not_pass_as_rows(self):
        def mutation(x):
            at=36+DEVICE_BYTES+8
            ptr=0xc30c0ff0
            struct.pack_into('<III',x,at,ptr+32,ptr+64,ptr+96)
        self.bad(mutation)
    def test_valid_indirect_bank_header_with_aligned_child_rows(self):
        source=bytearray(fake_capture())
        root=0xc30c0ff0
        device_at=36
        # The capture stores the ORIGINAL root pointer, not the nested
        # child address. A 76-byte header precedes the real 26-byte rows.
        base=36+DEVICE_BYTES
        nested=bytearray(76+2*ROW_BYTES)
        for i in range(7):
            struct.pack_into("<I",nested,4*i,root+76+200*i)
        struct.pack_into("<HBB",nested,76,1,7,1)
        struct.pack_into("<H",nested,76+ROW_BYTES,0xffff)
        struct.pack_into("<I",source,base+4,len(nested))
        source[base+8:]=nested
        report=parse_capture(bytes(source))
        self.assertEqual(
            report["source_rows_content_hashes"][0]["source_layout"],
            "indirect_bank_header")
        self.assertEqual(
            report["source_rows_content_hashes"][0]["active_rows"],1)
    def test_legacy_indirect_capture_ignores_unclassified_tail(self):
        # Reproduces the observed 16-Nov capture LAYOUT, never its bytes:
        # 76-byte root header + 129 original rows + FFFF + unrelated tail.
        source=bytearray(fake_capture())
        root=0xc30c0ff0
        base=36+DEVICE_BYTES
        header=bytearray(76)
        table=bytearray(130*ROW_BYTES)
        for index in range(129):
            struct.pack_into("<HBB",table,index*ROW_BYTES,1,index%128,0)
        struct.pack_into("<H",table,129*ROW_BYTES,0xffff)
        tail=bytes([0x37])*64
        for index,delta in enumerate((76,76+len(table),76+len(table)+64,
                                      76+len(table)+128,76+len(table)+192,
                                      76+len(table)+256,76+len(table)+320)):
            struct.pack_into("<I",header,index*4,root+delta)
        data=header+table+tail
        struct.pack_into("<I",source,base+4,len(data))
        source[base+8:]=data
        report=parse_capture(bytes(source))
        rows=report["source_rows_content_hashes"][0]
        self.assertEqual(rows["active_rows"],129)
        self.assertEqual(rows["total_rows_including_sentinel"],130)
        self.assertEqual(rows["source_layout"],"indirect_bank_header")
        self.assertEqual(rows["legacy_trailing_unclassified_bytes"],64)
    def test_legacy_indirect_capture_missing_child_sentinel(self):
        source=bytearray(fake_capture())
        root=0xc30c0ff0
        base=36+DEVICE_BYTES
        header=bytearray(76)
        for i in range(7):
            struct.pack_into("<I",header,i*4,root+76+100*i)
        rows=bytearray(2*ROW_BYTES)
        struct.pack_into("<H",rows,0,1)
        struct.pack_into("<H",rows,ROW_BYTES,0)
        data=header+rows
        struct.pack_into("<I",source,base+4,len(data))
        source[base+8:]=data
        with self.assertRaises(CaptureError):
            parse_capture(bytes(source))
    def test_wrong_source_row_marker_count(self):
        self.bad(lambda x:struct.pack_into("<I",x,28,0))
    def test_claimed_unreferenced_source_list(self):
        self.bad(lambda x:struct.pack_into("<I",x,36+DEVICE_BYTES,0xc0000000))

if __name__=="__main__":
    unittest.main(verbosity=2)
