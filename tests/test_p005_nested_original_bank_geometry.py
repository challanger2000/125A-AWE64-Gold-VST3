#!/usr/bin/env python3
"""Synthetic tests for P005 Creative nested memory index geometry."""
import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p005_analyze_original_nested_bank_geometry import analyze,GeometryError

ROOT=0xc30c0ff0
DEVICE=0x1592
SIZES=(12,9,8,16,12,4096)
# Three independently checkable relationships:
# 3 x u32 -> 3 x 3-byte records; 4 x u16 -> 4 x u32;
# 4 x u32 -> 4 x 3-byte records.
def fixture():
    device=bytearray(DEVICE)
    struct.pack_into("<H",device,6,32)
    struct.pack_into("<I",device,0x158e,0x4d474d31)
    struct.pack_into("<I",device,0x6ca,ROOT)
    headerbytes=76
    programbytes=52
    offsets=[headerbytes]
    for size in (programbytes,)+SIZES[:-1]:
        offsets.append(offsets[-1]+size)
    pointers=tuple(ROOT+x for x in offsets)
    rows=bytearray(headerbytes+programbytes)
    struct.pack_into("<7I",rows,0,*pointers)
    struct.pack_into("<HBB",rows,headerbytes,1,7,0)
    struct.pack_into("<H",rows,headerbytes+26,0xffff)
    struct.pack_into("<H",rows,headerbytes+26+4,2)
    primary=(b"125AVXD1"+
             struct.pack("<7I",1,0xc30b09f0,0x3000,DEVICE,288,1,1)+
             device+struct.pack("<II",ROOT,len(rows))+rows)
    data=[bytearray(n) for n in SIZES]
    struct.pack_into("<3I",data[0],0,0,1,3)
    struct.pack_into("<4H",data[2],0,0,1,2,3)
    struct.pack_into("<4I",data[3],0,0,1,3,4)
    nested=bytearray(b"125AVXR1"+
                     struct.pack("<4I",1,0xc30b09f0,0x3000,ROOT)+
                     struct.pack("<7I",*pointers)+
                     struct.pack("<I",6))
    locations=[]
    for idx,region in enumerate(data):
        locations.append(len(nested)+12)
        nested+=struct.pack("<3I",pointers[idx+1],len(region),int(idx<5))
        nested+=region
    return primary,nested,locations

def gm_fixture():
    # Entirely synthetic 129-program/181-bag/179-instrument geometry.
    # Programs refer to one synthetic instrument containing sampleID 0.
    lengths=(728,1398,360,2976,10932,4096)
    primary_first=76+130*26
    offsets=[76,primary_first]
    for length in lengths[:-1]:
        offsets.append(offsets[-1]+length)
    pointers=tuple(ROOT+offset for offset in offsets)
    device=bytearray(DEVICE)
    struct.pack_into("<H",device,6,32)
    struct.pack_into("<I",device,0x158e,0x4d474d31)
    struct.pack_into("<I",device,0x6ca,ROOT)
    root_bytes=bytearray(primary_first)
    struct.pack_into("<7I",root_bytes,0,*pointers)
    program_indices=[0]
    for n in range(129):
        program_indices.append(program_indices[-1]+(1 if n<77 else 2))
    assert program_indices[-1]==181
    for n in range(129):
        at=76+n*26
        struct.pack_into("<HBBH",root_bytes,at,1,n,0,program_indices[n])
    struct.pack_into("<H",root_bytes,76+129*26,0xffff)
    struct.pack_into("<H",root_bytes,76+129*26+4,181)
    primary=(b"125AVXD1"+
             struct.pack("<7I",1,0xc30b09f0,0x3000,DEVICE,288,1,1)+
             device+struct.pack("<II",ROOT,len(root_bytes))+root_bytes)
    parts=[bytearray(n) for n in lengths]
    preset_offsets=list(range(181))+[466]
    struct.pack_into("<182I",parts[0],0,*preset_offsets)
    for n in range(181):
        at=(preset_offsets[n+1]-1)*3
        parts[1][at:at+3]=bytes([41,0,0])
    instrument_to_bags=[0,1]+[1]*177+[743]
    struct.pack_into("<180H",parts[2],0,*instrument_to_bags)
    bag_to_generators=[0,1]+[1]*741+[3644]
    struct.pack_into("<744I",parts[3],0,*bag_to_generators)
    parts[4][0:3]=bytes([53,0,0])
    nested=bytearray(b"125AVXR1"+
                     struct.pack("<4I",1,0xc30b09f0,0x3000,ROOT)+
                     struct.pack("<7I",*pointers)+
                     struct.pack("<I",6))
    positions=[]
    for i,part in enumerate(parts):
        positions.append(len(nested)+12)
        nested+=struct.pack("<3I",pointers[i+1],len(part),int(i<5))
        nested+=part
    return primary,nested,positions

class GeometryTests(unittest.TestCase):
    def test_full_original_gm_index_path_to_sample_id(self):
        p,n,_=gm_fixture()
        results=analyze(p,bytes(n))
        path=results["owner_gm_source_paths"]
        self.assertEqual(path["programs_with_source_sample_id_references"],129)
        self.assertEqual(path["unique_instrument_indices"],1)
        self.assertEqual(path["sample_id_references_including_aliases"],181)
        self.assertEqual(path["unique_sample_id_numbers"],1)
        self.assertFalse(path["creative_noteon_audio_parity_proven"])
    def test_owner_gm_missing_sample_id_fails(self):
        p,n,positions=gm_fixture()
        n[positions[4]]=0
        with self.assertRaises(GeometryError):
            analyze(p,bytes(n))
    def test_owner_gm_bad_instrument_id_fails(self):
        p,n,positions=gm_fixture()
        # First three-byte preset record is op=41 instrument=65535.
        n[positions[1]+1:positions[1]+3]=bytes([255,255])
        with self.assertRaises(GeometryError):
            analyze(p,bytes(n))
    def test_valid_three_links(self):
        p,n,_=fixture()
        result=analyze(p,bytes(n))
        self.assertEqual(len(result["observed_structural_relations"]),3)
        self.assertEqual(
            result["original_program_to_nested_index"]
            ["last_original_sentinel_index"],2)
        self.assertTrue(all(x["relation"]=="EXACT_MATCH"
                            for x in result["observed_structural_relations"]))
        self.assertFalse(result["table_semantics_independently_proven"])
        self.assertFalse(result["vst3_production_authorized"])
    def corrupt(self,part,offset,payload):
        p,n,places=fixture()
        n[places[part]+offset:places[part]+offset+len(payload)]=payload
        with self.assertRaises(GeometryError):
            analyze(p,bytes(n))
    def test_original_program_row_missing_nested_link(self):
        p,n,_=fixture()
        altered=bytearray(p)
        source_start=36+DEVICE+8+76
        struct.pack_into("<H",altered,source_start+26+4,1)
        # A valid primary binary can still contain an incorrect
        # instrument index chain; only the geometry stage rejects it.
        with self.assertRaises(GeometryError):
            analyze(bytes(altered),bytes(n))
    def test_original_preset_index_not_zero(self):
        p,n,_=fixture()
        altered=bytearray(p)
        source_start=36+DEVICE+8+76
        struct.pack_into("<H",altered,source_start+4,1)
        with self.assertRaises(GeometryError):
            analyze(bytes(altered),bytes(n))
    def test_packed_generator_invalid_opcode_rejected(self):
        self.corrupt(1,0,bytes([255]))
    def test_packed_instrument_index_out_of_range_rejected(self):
        # A 3-byte record can advertise op 41, but its u16 target
        # must never name a nonexistent original instrument index.
        self.corrupt(1,0,bytes([41,255,255]))
    def test_packed_generator_diagnostic_exposes_unproven_semantics(self):
        p,n,places=fixture()
        n[places[1]:places[1]+3]=bytes([41,0,0])
        result=analyze(p,bytes(n))
        info=result["packed_generator_evidence"]
        self.assertEqual(info["preset_instrument_opcode_41_count"],1)
        self.assertFalse(info["actual_creative_instruction_semantics_proven"])
    def test_index_not_monotonic(self):
        self.corrupt(0,8,struct.pack("<I",0))
    def test_index_out_of_bounds(self):
        self.corrupt(0,8,struct.pack("<I",4))
    def test_second_chain_terminal_mismatch(self):
        self.corrupt(2,6,struct.pack("<H",2))
    def test_third_chain_terminal_mismatch(self):
        self.corrupt(3,12,struct.pack("<I",3))
    def test_index_starts_at_nonzero(self):
        self.corrupt(3,0,struct.pack("<I",1))

if __name__=="__main__":
    unittest.main(verbosity=2)
