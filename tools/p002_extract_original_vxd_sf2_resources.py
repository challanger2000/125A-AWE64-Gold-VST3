#!/usr/bin/env python3
"""Extract the *private* original SBAWE.VXD SF2 template and pitch tables.

Run only against the original 77,370-byte Windows 95 driver identified by
SHA-256. The output contains source-derived proprietary driver constants:
KEEP THE OUTPUT IN THE PRIVATE TEST LAB; NEVER COMMIT OR UPLOAD IT.

This reads exact LE page maps. An MZ or ordinary PE/raw-offset read is NOT
an adequate VxD parser. Fixups are not needed for these constant-only spans.
It does not emulate PCOD+0x3735 or claim sample-rate/pitch parity.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

EXPECTED_VXD_SHA256 = (
    "d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a"
)
EXPECTED_OBJECTS = (
    ("LCOD", 0x79E0, 1, 61),
    ("MCOD", 0x37, 62, 1),
    ("PNPC", 0x1DD8, 70, 15),
    ("PCOD", 0x5A55, 85, 46),
    ("PDAT", 0xDE8, 131, 7),
)
SCHEMA = "125a-private-original-creative-sf2-resources-v1"


class InvalidCreativeVxd(ValueError):
    """Unexpected file, page map, or object boundary: reject without guessing."""


def u32(data: bytes, off: int) -> int:
    if off < 0 or off + 4 > len(data):
        raise InvalidCreativeVxd("Truncated LE header or object entry")
    return struct.unpack_from("<I", data, off)[0]


def extract(raw: bytes, *, require_original_hash: bool = True) -> dict:
    digest = hashlib.sha256(raw).hexdigest()
    if require_original_hash and digest != EXPECTED_VXD_SHA256:
        raise InvalidCreativeVxd("Not the exact original private SBAWE.VXD")
    if len(raw) < 0x180 or raw[:2] != b"MZ":
        raise InvalidCreativeVxd("Missing DOS MZ wrapper")
    le = u32(raw, 0x3C)
    if le + 0xA8 > len(raw) or raw[le:le + 2] != b"LE":
        raise InvalidCreativeVxd("Missing native 32-bit LE header")
    if raw[le + 2:le + 4] != bytes(2):
        raise InvalidCreativeVxd("Unsupported byte/word order")
    page_size = u32(raw, le + 0x28)
    obj_table = u32(raw, le + 0x40)
    num_objects = u32(raw, le + 0x44)
    page_map = u32(raw, le + 0x48)
    data_offset = u32(raw, le + 0x80)  # absolute FILE offset
    if (le != 0x80 or page_size != 512 or num_objects != 5 or
            data_offset != 0x1A00):
        raise InvalidCreativeVxd("Wrong original LE layout / page size")
    if (obj_table < 0xA8 or page_map <= obj_table or
            le + obj_table + num_objects * 24 > len(raw) or
            le + page_map + 137 * 4 > data_offset):
        raise InvalidCreativeVxd("LE object/page tables outside loader region")
    objects: dict[str, bytes] = {}
    for n, (name, expected_size, expected_first, expected_pages) in enumerate(
            EXPECTED_OBJECTS):
        pos = le + obj_table + n * 24
        size, _reloc, flags, first, count, _reserved = struct.unpack_from(
            "<6I", raw, pos)
        if (size, first, count) != (
                expected_size, expected_first, expected_pages):
            raise InvalidCreativeVxd(f"{name}: source object geometry changed")
        if not (flags & 0x2000):
            raise InvalidCreativeVxd(f"{name}: not 32-bit LE")
        image = bytearray()
        for i in range(count):
            map_off = le + page_map + (first - 1 + i) * 4
            if map_off + 4 > data_offset:
                raise InvalidCreativeVxd(f"{name}: page map out of bounds")
            entry = raw[map_off:map_off + 4]
            # LE page number = 24-bit BIG-endian bytes, then flags.
            # Reading all four as a little-endian uint32 is incorrect.
            page_no = int.from_bytes(entry[:3], "big")
            if page_no < 1 or page_no > 137 or entry[3] != 0:
                raise InvalidCreativeVxd(f"{name}: unsupported page map entry")
            file_off = data_offset + (page_no - 1) * page_size
            page = raw[file_off:file_off + page_size]
            need = min(page_size, max(0, size - len(image)))
            if len(page) < need:
                raise InvalidCreativeVxd(f"{name}: source page truncated")
            image.extend(page[:need])
        if len(image) != size:
            raise InvalidCreativeVxd(f"{name}: wrong virtual byte count")
        objects[name] = bytes(image)
    lcod, pdat = objects["LCOD"], objects["PDAT"]
    # Actual source-derived offsets, not offsets into the DOS stub or PE.
    # Format2 descriptor static template has a MUTABLE final DWORD.
    template = lcod[0x2D88:0x2D88 + 0x94]
    # Original x86 PCOD+0x3735, verified from actual driver instructions:
    #   ADD EAX, DWORD PTR [index*4 + PDAT+0x8e8] (128 unsigned DWORDs)
    #   MOVSX EAX, BYTE PTR [index + PDAT+0xae8] (128 signed BYTEs)
    # The remaining bytes beyond +0xae8+128 belong to other PDAT data.
    # Original LCOD+0x5E90 uses a 16-WORD table of volume-target\n    # attenuation factors. Source x86 indexed by abs(gen19)&15.\n    # Its values remain PRIVATE; no source table is committed.\n    volume_attack_table = lcod[0x2FA0:0x2FA0 + 16 * 2]\n    table_a = pdat[0x08E8:0x08E8 + 128 * 4]
    table_b = pdat[0x0AE8:0x0AE8 + 128]
    if len(volume_attack_table) != 32:\n        raise InvalidCreativeVxd("Original LCOD volume lookup truncated")\n    if len(template) != 148 or len(table_a) != 512 or len(table_b) != 128:
        raise InvalidCreativeVxd("Original format-2 resource bounds invalid")
    values = list(struct.unpack("<59h", template[:118]))
    words_a = list(struct.unpack("<128I", table_a))
    words_b = list(struct.unpack("<128b", table_b))
    # The template's final DWORD is rewritten for EACH normalized preset.
    # Do not export it as a soundbank-independent default for dwSPadId.
    return {
        "schema": SCHEMA,
        "source_vxd_sha256": digest,
        "source_le_object_sizes": {key: len(v) for key, v in objects.items()},
        "template_lcod_offset": "0x2d88",
        "template_first_59_signed_words": values,
        "template_sha256": hashlib.sha256(template).hexdigest(),
        "template_group_id_is_mutable": True,
        "volume_attack128_lcod_offset": "0x2fa0",\n        "volume_attack128_table_unsigned_16": list(\n            struct.unpack("<16H", volume_attack_table)\n        ),\n        "volume_attack128_table_sha256": hashlib.sha256(\n            volume_attack_table\n        ).hexdigest(),\n        "pitch_pdat_table_a_offset": "0x08e8",
        "pitch_pdat_table_b_offset": "0x0ae8",
        "pitch_table_a_unsigned_32": words_a,
        "pitch_table_b_signed_8": words_b,
        "pitch_table_a_sha256": hashlib.sha256(table_a).hexdigest(),
        "pitch_table_b_sha256": hashlib.sha256(table_b).hexdigest(),
        "pitch_conversion_pc0d_0x3735_executed": False,
        "original_pcm_parity_proven": False,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--private-vxd", required=True, type=Path)
    parser.add_argument("--private-output", required=True, type=Path)
    args = parser.parse_args()
    if args.private_vxd.resolve() == args.private_output.resolve():
        parser.error("Output may not overwrite the original driver")
    result = extract(args.private_vxd.read_bytes())
    args.private_output.parent.mkdir(parents=True, exist_ok=True)
    if args.private_output.exists():
        parser.error("Refusing to overwrite existing private test vector")
    # 0600 even under a permissive user's umask. Do not use tempfile to
    # publish original table bytes into a shared reports directory.
    import os
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    fd = os.open(args.private_output, flags, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as out:
        json.dump(result, out, indent=2, sort_keys=True)
        out.write("\n")
    print("P002_PRIVATE_LE_ORIGINAL_SF2_RESOURCES_EXTRACTED")
    print("vxd_sha256=" + result["source_vxd_sha256"])
    print("template_sha256=" + result["template_sha256"])
    print("pitch_table_a_sha256=" + result["pitch_table_a_sha256"])
    print("pitch_table_b_sha256=" + result["pitch_table_b_sha256"])
    print("actual_dwPitch_algorithm=NOT_YET_COMPLETED")


if __name__ == "__main__":
    main()
