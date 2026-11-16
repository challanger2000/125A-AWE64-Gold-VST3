import importlib.util
from pathlib import Path
import struct
import unittest
P=Path(__file__).resolve().parent.parent/"tools/p002_sbk_descriptor_probe.py"
spec=importlib.util.spec_from_file_location("p002_sbk",P)
m=importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

def chunk(name,payload):
    return name+struct.pack("<I",len(payload))+payload+(b"\x00" if len(payload)&1 else b"")
def name20(text):
    return text.encode("ascii").ljust(20,b"\x00")
def fixture():
    # ORIGINAL test fixture with metadata-only fictional rows, NO Creative
    # copyrighted bank/sample/ROM bytes from the user's private source.
    phdr=(
        struct.pack("<20sHHHIII",name20("TEST0"),0,0,0,0,0,0)+
        struct.pack("<20sHHHIII",name20("EOP"),0,0,1,0,0,0))
    pbag=struct.pack("<HHHH",0,0,2,0)
    pgen=struct.pack("<HHHH",48,107,41,0)
    inst=struct.pack("<20sH",name20("ZONE"),0)+struct.pack("<20sH",name20("EOI"),2)
    ibag=struct.pack("<HHHHHH",0,0,1,0,4,0)
    igen=struct.pack("<HHHHHHHH",8,110,43,0x423A,8,89,53,3)
    pdta=b"pdta"+b"".join(chunk(*x) for x in (
        (b"phdr",phdr),(b"pbag",pbag),(b"pgen",pgen),
        (b"inst",inst),(b"ibag",ibag),(b"igen",igen),
        (b"shdr",b"\x00"*96)))
    info=b"INFO"+chunk(b"isng",b"EMU8000\x00")+chunk(b"IPRD",b"SBAWE32\x00")
    whole=chunk(b"LIST",info)+chunk(b"LIST",pdta)
    return b"RIFF"+struct.pack("<I",len(whole)+4)+b"sfbk"+whole

class CreativeSbkOracleContract(unittest.TestCase):
    def test_synthetic_original_structure_matches_correct_key_zone(self):
        raw=fixture()
        r=m.descriptor(raw,0,0,60)
        self.assertEqual(r["zone_raw_cutoff"],89)
        self.assertEqual(r["global_raw_cutoff"],110)
        self.assertEqual(r["original_measured_chip_cutoff_candidate"],178)
        self.assertEqual(r["original_preset_attenuation_raw"],107)
        self.assertEqual(r["key_range"],[58,66])
    def test_wrong_key_fails_instead_of_using_global_preset_cutoff(self):
        with self.assertRaisesRegex(ValueError,"ambiguity"):
            m.descriptor(fixture(),0,0,72)
    def test_wrong_bank_and_program_fail(self):
        with self.assertRaisesRegex(ValueError,"ambiguity"):
            m.descriptor(fixture(),40,0,60)
        with self.assertRaisesRegex(ValueError,"ambiguity"):
            m.descriptor(fixture(),0,1,60)
    def test_original_creative_48_byte_shdr_not_standard_46(self):
        raw=bytearray(fixture())
        needle=b"shdr"+struct.pack("<I",96)
        where=raw.index(needle)
        # Changing declared stride will make this chunk invalid even
        # when a generic SF2 parser would accept a 46-byte record size.
        raw[where+4:where+8]=struct.pack("<I",92)
        with self.assertRaises(ValueError):
            m.descriptor(bytes(raw),0,0,60)
    def test_incomplete_riff_fails(self):
        with self.assertRaisesRegex(ValueError,"RIFF"):
            m.descriptor(fixture()[:-1],0,0,60)
    def test_wrong_device_product_metadata_fails(self):
        with self.assertRaisesRegex(ValueError,"not original Creative"):
            m.descriptor(fixture().replace(b"SBAWE32",b"FAKE123"),0,0,60)

if __name__=="__main__":unittest.main()
