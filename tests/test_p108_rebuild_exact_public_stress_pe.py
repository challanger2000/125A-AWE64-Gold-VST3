"""Safe fictional PE32 metadata-only tests. NO Win95/Creative/ROM/owner samples."""
import sys
import struct
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p108_rebuild_exact_public_stress_pe import (
    restore_public_pe_original_metadata,
    exact_public_guest_from_compiled,
    ORIGINAL_PUBLIC_PE32_BYTES,
    ORIGINAL_COFF_TIMESTAMP,
    ORIGINAL_OPTIONAL_CHECKSUM
)
from p108_compare_public_stress_pe import parse_public_pe32,sha

def fake_public_test_pe():
    p=bytearray(ORIGINAL_PUBLIC_PE32_BYTES)
    p[:2]=b"MZ"
    struct.pack_into("<I",p,0x3c,128)
    p[128:132]=b"PE\x00\x00"
    struct.pack_into("<HHIIIHH",p,132,0x14c,1,12345,0,0,224,0x102)
    struct.pack_into("<H",p,152,0x10b)
    table=128+24+224
    p[table:table+8]=b".text\x00\x00\x00"
    struct.pack_into("<I",p,table+16,320)
    struct.pack_into("<I",p,table+20,1024)
    p[1024:1344]=bytes(x%256 for x in range(320))
    struct.pack_into("<I",p,128+24+64,8799)
    return bytes(p)

class TestOriginalPublicPE32Restoration(unittest.TestCase):
    def test_only_two_public_metadata_words_are_modified(self):
        original=fake_public_test_pe()
        result=restore_public_pe_original_metadata(original)
        self.assertEqual(len(result),ORIGINAL_PUBLIC_PE32_BYTES)
        before=parse_public_pe32(original)
        after=parse_public_pe32(result)
        self.assertEqual(before["sections"],after["sections"])
        self.assertEqual(after["coff_timestamp"],ORIGINAL_COFF_TIMESTAMP)
        self.assertEqual(after["optional_checksum"],ORIGINAL_OPTIONAL_CHECKSUM)
        intervals=set(range(136,140))|set(range(216,220))
        actual={i for i,(x,y) in enumerate(zip(original,result)) if x!=y}
        self.assertTrue(actual<=intervals)
        self.assertGreater(len(actual),0)
    def test_no_hypothetical_guest_gets_original_sha_without_whole_file_match(self):
        with self.assertRaisesRegex(ValueError,"differs beyond authenticated"):
            exact_public_guest_from_compiled(fake_public_test_pe())
    def test_code_section_mutation_not_accepted_as_original(self):
        original=bytearray(fake_public_test_pe())
        original[1026]^=0x55
        with self.assertRaisesRegex(ValueError,"differs beyond authenticated"):
            exact_public_guest_from_compiled(bytes(original))
    def test_invalid_pe_header_and_truncated_image_rejected(self):
        with self.assertRaisesRegex(ValueError,"MZ"):
            restore_public_pe_original_metadata(b"invalid")
        wrong=bytearray(fake_public_test_pe())
        wrong[128:132]=b"FAKE"
        with self.assertRaisesRegex(ValueError,"PE signature"):
            restore_public_pe_original_metadata(bytes(wrong))
    def test_rewrite_is_idempotent_but_not_original_without_sha(self):
        x=restore_public_pe_original_metadata(fake_public_test_pe())
        self.assertEqual(restore_public_pe_original_metadata(x),x)
        self.assertNotEqual(sha(x),"3c9306261ed599073f80d9695a8d73140ea65e2de101dc431169c2e6b3f6bc9f")

if __name__=="__main__":unittest.main()
