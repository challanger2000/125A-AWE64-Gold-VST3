"""Contract checks for original Creative LE object resources.

Creates ONLY synthetic LE pages. Never checks in the original driver.
"""
import hashlib
import struct
import unittest
from tools.p002_extract_original_vxd_sf2_resources import (
    EXPECTED_OBJECTS,
    InvalidCreativeVxd,
    extract,
)

LE = 0x80
PAGE = 512
DATA = 0x1A00
OBJECT_TABLE = 0xB0
PAGE_MAP = 0x200


def synthetic_le(*, corrupt_page: int | None = None) -> bytes:
    raw = bytearray(DATA + 137 * PAGE)
    raw[:2] = b"MZ"
    struct.pack_into("<I", raw, 0x3C, LE)
    raw[LE:LE + 4] = b"LE\x00\x00"
    struct.pack_into("<I", raw, LE + 0x28, PAGE)
    struct.pack_into("<I", raw, LE + 0x40, OBJECT_TABLE)
    struct.pack_into("<I", raw, LE + 0x44, len(EXPECTED_OBJECTS))
    struct.pack_into("<I", raw, LE + 0x48, PAGE_MAP)
    struct.pack_into("<I", raw, LE + 0x80, DATA)
    for idx, (_, virtual_size, first, count) in enumerate(EXPECTED_OBJECTS):
        struct.pack_into(
            "<6I", raw, LE + OBJECT_TABLE + idx * 24,
            virtual_size, 0, 0x2000, first, count, 0,
        )
        for p in range(first, first + count):
            # Three-byte BIG-endian physical page id followed by LE flags.
            off = LE + PAGE_MAP + (p - 1) * 4
            raw[off:off + 4] = p.to_bytes(3, "big") + b"\x00"
    if corrupt_page:
        off = LE + PAGE_MAP + (corrupt_page - 1) * 4
        raw[off + 3] = 1
    # Exact original resource positions within respective OBJECT images.
    templ = struct.pack("<59h", *range(-29, 30)) + bytes(30)
    templ = templ[:144] + b"RNDM"  # final DWORD is dynamic, not static
    # LCOD starts at physical page1: virtual offset==file-relative offset
    raw[DATA + 0x2D88:DATA + 0x2D88 + 148] = templ
    synthetic_volume = struct.pack("<16H", *(i*137 for i in range(16)))\n    raw[DATA + 0x2FA0:DATA + 0x2FA0 + 32] = synthetic_volume\n    # PDAT starts physical page131.
    pd = DATA + (131 - 1) * PAGE
    table1 = struct.pack("<128I", *(1000 + i*10000 for i in range(128)))
    table2 = struct.pack("<128b", *(64 - i//2 for i in range(128)))
    raw[pd + 0x08E8:pd + 0x08E8 + 512] = table1
    raw[pd + 0x0AE8:pd + 0x0AE8 + 128] = table2
    return bytes(raw)


class CreativeVxdPrivateResourcesContract(unittest.TestCase):
    def test_five_original_le_objects_exact_59_words_and_two_tables(self):
        raw = synthetic_le()
        result = extract(raw, require_original_hash=False)
        self.assertEqual(
            result["schema"], "125a-private-original-creative-sf2-resources-v1"
        )
        self.assertEqual(result["template_first_59_signed_words"],
                         list(range(-29, 30)))
        self.assertEqual(result["volume_attack128_table_unsigned_16"],\n                         [i*137 for i in range(16)])\n        self.assertEqual(result["volume_attack128_lcod_offset"],"0x2fa0")\n        self.assertEqual(\n            result["volume_attack128_table_sha256"],\n            hashlib.sha256(struct.pack("<16H",*(i*137 for i in range(16)))).hexdigest()\n        )\n        self.assertEqual(result["pitch_table_a_unsigned_32"],
                         [1000+i*10000 for i in range(128)])
        self.assertEqual(result["pitch_table_b_signed_8"],
                         [64-i//2 for i in range(128)])
        self.assertEqual(
            result["source_le_object_sizes"],
            {name: size for name, size, _, _ in EXPECTED_OBJECTS},
        )
        self.assertEqual(result["source_vxd_sha256"],
                         hashlib.sha256(raw).hexdigest())
        self.assertTrue(result["template_group_id_is_mutable"])
        self.assertFalse(result["pitch_conversion_pc0d_0x3735_executed"])
        self.assertFalse(result["original_pcm_parity_proven"])

    def test_synthetic_driver_is_never_accepted_as_original(self):
        with self.assertRaisesRegex(InvalidCreativeVxd, "exact original"):
            extract(synthetic_le())

    def test_mz_and_le_and_endian_must_be_valid(self):
        data = bytearray(synthetic_le())
        data[0] = 0
        with self.assertRaisesRegex(InvalidCreativeVxd, "MZ"):
            extract(bytes(data), require_original_hash=False)
        data = bytearray(synthetic_le())
        data[LE] = 0
        with self.assertRaisesRegex(InvalidCreativeVxd, "LE"):
            extract(bytes(data), require_original_hash=False)
        data = bytearray(synthetic_le())
        data[LE + 2] = 1
        with self.assertRaisesRegex(InvalidCreativeVxd, "order"):
            extract(bytes(data), require_original_hash=False)

    def test_page_flags_and_object_geometry_fail_closed(self):
        with self.assertRaisesRegex(InvalidCreativeVxd, "page map entry"):
            extract(synthetic_le(corrupt_page=131),
                    require_original_hash=False)
        data = bytearray(synthetic_le())
        struct.pack_into("<I", data, LE + OBJECT_TABLE + 0 * 24, 0x1234)
        with self.assertRaisesRegex(InvalidCreativeVxd, "geometry"):
            extract(bytes(data), require_original_hash=False)

    def test_wrong_page_size_and_truncated_page_are_rejected(self):
        data = bytearray(synthetic_le())
        struct.pack_into("<I", data, LE + 0x28, 4096)
        with self.assertRaisesRegex(InvalidCreativeVxd, "layout"):
            extract(bytes(data), require_original_hash=False)
        data = bytearray(synthetic_le()[:-2000])
        with self.assertRaisesRegex(InvalidCreativeVxd, "truncated"):
            extract(bytes(data), require_original_hash=False)

    def test_wrong_le_object_page_map_table_bounds_are_rejected(self):
        data = bytearray(synthetic_le())
        struct.pack_into("<I", data, LE + 0x48, 0x19FF)
        with self.assertRaisesRegex(InvalidCreativeVxd, "outside"):
            extract(bytes(data), require_original_hash=False)


if __name__ == "__main__":
    unittest.main()
