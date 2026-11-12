"""Fail-closed PRIVATE NoteOff release fixture v2 format.

All values are synthetic; this validates schema and source-record byte
transfer only. No real SBAWE.VXD or ROM is bundled or executed.
"""
import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
import p002_make_private_vst3_note_fixture as mod

def data():
    desc=bytearray(148)
    desc[0x8c]=1
    return (
        {"schema":mod.SCHEMA,
         "original_vxd_sha256":mod.EXPECTED_VXD_SHA256,
         "original_bank_sha256":mod.BANK_SHA,
         "cases":[{"case":{
             "program":0,"bank":0,"key":60,"velocity":90,
             "physical_voices":[0],"after":[desc.hex()],
             "state":{"rom":"1MGM","effects_flag":0,
                      "device_attenuation":0,"cc7":100,"cc11":127}}}]},
        {"source_vxd_sha256":mod.EXPECTED_VXD_SHA256,
         "volume_attack128_negative_table_unsigned_16":[0]*15,
         "volume_attack128_table_unsigned_16":[0]*16}
    )
def release():
    record=bytearray(32)
    struct.pack_into("<H",record,0,60)
    record[0x15]=45
    record[0x16]=92
    return {"schema":"125a-original-win95-vxd-release-records-v1",
            "original_vxd_sha256":mod.EXPECTED_VXD_SHA256,
            "original_bank_sha256":mod.BANK_SHA,
            "program":0,"channel":0,"key":60,"velocity":90,
            "records":[{"physical_voice":0,
                        "original_vxd_voice_32_hex":record.hex()}]}

class PrivateOriginalNoteOffFixture(unittest.TestCase):
    def test_v1_keeps_noteoff_disabled(self):
        h,s=data()
        got=mod.generate(h,s,0)
        self.assertEqual(got[:8],mod.MAGIC)
        self.assertEqual(got[8],1)
        self.assertEqual(got[8+6+64+0x42],0)

    def test_v3_automatic_original_noteon_release(self):
        h,s=data()
        h["cases"][0]["case"]["after"][0]=(
            lambda b:b.hex()
        )(self.make_descriptor())
        got=mod.generate(h,s,0,derive_noteoff_from_original_descriptor=True)
        self.assertEqual(got[8],3)
        self.assertEqual(got[:8],mod.MAGIC)

    @staticmethod
    def make_descriptor():
        d=bytearray(148)
        d[0x8c]=1
        d[0x3c]=127
        d[0x4c]=41
        struct.pack_into("<H",d,2*54,1)
        return d

    def test_v3_and_explicit_record_mutually_exclusive(self):
        h,s=data()
        with self.assertRaisesRegex(ValueError,"not both"):
            mod.generate(h,s,0,release(),True)

    def test_v2_copies_exact_record_bytes(self):
        h,s=data()
        r=release()
        got=mod.generate(h,s,0,r)
        self.assertEqual(got[8],2)
        offset=8+6+64+0x42
        b=bytes.fromhex(r["records"][0]["original_vxd_voice_32_hex"])
        self.assertEqual(got[offset:offset+32],b)
        self.assertEqual(got[offset+0x15],45)
        self.assertEqual(got[offset+0x16],92)

    def test_wrong_original_driver_sha_rejected(self):
        h,s=data()
        r=release()
        r["original_vxd_sha256"]="0"*64
        with self.assertRaisesRegex(ValueError,"identity mismatch"):
            mod.generate(h,s,0,r)

    def test_wrong_voice_or_duplicate_rejected(self):
        h,s=data()
        r=release()
        r["records"][0]["physical_voice"]=3
        with self.assertRaisesRegex(ValueError,"physical voice mismatch"):
            mod.generate(h,s,0,r)
        r=release()
        r["records"].append(r["records"][0])
        with self.assertRaisesRegex(ValueError,"voice count mismatch"):
            mod.generate(h,s,0,r)

    def test_wrong_note_marker_and_encoded_rates_rejected(self):
        h,s=data()
        r=release()
        b=bytearray.fromhex(r["records"][0]["original_vxd_voice_32_hex"])
        b[0]=61
        r["records"][0]["original_vxd_voice_32_hex"]=b.hex()
        with self.assertRaisesRegex(ValueError,"marker/rates/end"):
            mod.generate(h,s,0,r)
        b[0]=60
        b[0x16]=128
        r["records"][0]["original_vxd_voice_32_hex"]=b.hex()
        with self.assertRaisesRegex(ValueError,"marker/rates/end"):
            mod.generate(h,s,0,r)

    def test_rewrite_end_must_fit_original_24bit_chip(self):
        h,s=data()
        r=release()
        b=bytearray.fromhex(r["records"][0]["original_vxd_voice_32_hex"])
        struct.pack_into("<I",b,6,0xfffffc)
        r["records"][0]["original_vxd_voice_32_hex"]=b.hex()
        with self.assertRaisesRegex(ValueError,"marker/rates/end"):
            mod.generate(h,s,0,r)

    def test_malformed_source_record_hex_rejected(self):
        h,s=data()
        r=release()
        r["records"][0]["original_vxd_voice_32_hex"]="not-hex"
        with self.assertRaisesRegex(ValueError,"release bytes"):
            mod.generate(h,s,0,r)

if __name__=="__main__":
    unittest.main()
