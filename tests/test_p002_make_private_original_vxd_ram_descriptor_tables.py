"""Synthetic-only regression of PRIVATE Creative VXD hash-locked PCOD stage."""
from pathlib import Path
import hashlib
import json
import struct
import tempfile
import unittest
from unittest.mock import patch
from tools import p002_make_private_original_vxd_ram_descriptor_tables as stage

class PrivateOriginalVxdTablePackTests(unittest.TestCase):
    @staticmethod
    def synthetic_resource():
        return {
            "template_first_59_signed_words":[0]*59,
            "pitch_table_a_unsigned_32":[i*123 for i in range(128)],
            "pitch_table_b_signed_8":[-2+(i%5) for i in range(128)],
            "volume_attack128_negative_table_unsigned_16":[i+1 for i in range(15)],
            "volume_attack128_table_unsigned_16":[i+1 for i in range(16)],
        }

    def test_original_resource_byte_layout_820_no_rom_or_sf2(self):
        a=self.synthetic_resource()
        data=stage.make_original_table_bytes(a)
        self.assertEqual(len(data),820)
        self.assertEqual(struct.unpack_from("<I",data,118+4)[0],123)
        self.assertEqual(struct.unpack_from("<b",data,118+512)[0],-2)
        self.assertEqual(struct.unpack_from("<H",data,758)[0],1)
        self.assertEqual(struct.unpack_from("<H",data,788)[0],1)
        self.assertNotEqual(hashlib.sha256(data).hexdigest(),
                            stage.EXPECTED_TABLE_SHA256)

    def test_tampered_pitch_or_volume_cannot_masquerade_as_source(self):
        base=self.synthetic_resource()
        with patch.object(stage,"extract",return_value=base):
            with self.assertRaisesRegex(ValueError,"table SHA mismatch"):
                stage.private_table_from_original_vxd(b"synthetic")
        changed=self.synthetic_resource()
        changed["volume_attack128_table_unsigned_16"][7]^=1
        self.assertNotEqual(stage.make_original_table_bytes(base),
                            stage.make_original_table_bytes(changed))

    def test_invalid_source_le_page_mapping_rejected(self):
        with self.assertRaises(ValueError):
            stage.private_table_from_original_vxd(bytes(77370))

    def test_wrong_table_geometry_cannot_be_padded_silently(self):
        a=self.synthetic_resource()
        a["pitch_table_b_signed_8"].pop()
        with self.assertRaises(struct.error):
            stage.make_original_table_bytes(a)
        a=self.synthetic_resource()
        a["template_first_59_signed_words"][0]=32768
        with self.assertRaises(struct.error):
            stage.make_original_table_bytes(a)

    def test_refuse_any_existing_private_output_even_before_source_read(self):
        with tempfile.TemporaryDirectory(prefix="p002-vxd-pack-") as t:
            root=Path(t)
            source=root/"private.vxd"
            source.write_bytes(b"this is NOT the original Creative VXD")
            output=root/"must-not-overwrite.bin"
            output.write_bytes(b"ORIGINAL")
            with self.assertRaises(SystemExit):
                stage.main(["--private-original-vxd",str(source),
                            "--private-output",str(output)])
            self.assertEqual(output.read_bytes(),b"ORIGINAL")

    def test_no_partial_output_on_forged_vxd(self):
        with tempfile.TemporaryDirectory(prefix="p002-vxd-no-go-") as t:
            root=Path(t)
            source=root/"private.vxd"
            source.write_bytes(b"NOT GENUINE SBAWE")
            output=root/"never-created.bin"
            with self.assertRaises(ValueError):
                stage.main(["--private-original-vxd",str(source),
                            "--private-output",str(output)])
            self.assertFalse(output.exists())

if __name__=="__main__":
    unittest.main()
