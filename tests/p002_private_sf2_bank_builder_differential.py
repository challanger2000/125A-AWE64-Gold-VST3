#!/usr/bin/env python3
"""Private original VXD format-2 59-generator C++ builder differential.

This test reads two independent PRIVATE user-supplied inputs:
(1) the SHA-256 pinned original 77,370-byte LE VXD's format2 template,
(2) the original Windows95 executed SF2 before-NoteOn handoff.
No driver/template/SF2 bytes are embedded in the public repository or logs.
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path

# CTest executes from build-p002; ensure the repository's existing tools/
# package is found without relying on a user-specific PYTHONPATH.
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.p002_extract_original_vxd_sf2_resources import (
    EXPECTED_VXD_SHA256, extract,
)


def private_rows(handoff: dict, driver: bytes):
    if handoff.get("schema") != "125a-original-bank-to-register-handoff-v1":
        raise ValueError("Wrong source original bank handoff schema")
    if handoff.get("original_vxd_sha256") != EXPECTED_VXD_SHA256:
        raise ValueError("Original VXD provenance mismatch")
    if handoff.get("original_bank_sha256") != (
        "78f8739a25d49e519b3fdc92a7f03d4ddd68588fd0260578a880296e304b7b5f"
    ):
        raise ValueError("Original ROM SF2 provenance mismatch")
    result = extract(driver)  # exact private LE hash and object geometry
    original_instrument_template = result["template_first_59_signed_words"]
    if len(original_instrument_template) != 59:
        raise ValueError("Wrong original format2 template word count")
    # The builder's PRESET stage starts zero, before adding actual
    # preset-zone generator contributions. The instrument stage uses the
    # genuine original LCOD+0x2D88 template; never invent template defaults.
    original_preset_template = [0] * 59
    rows = []
    expected = []
    labels = []
    for item in handoff["cases"]:
        case = item["case"]
        before = case["before"]
        if len(before) != len(item["layers"]):
            raise ValueError("Missing original 148-byte pre-start descriptors")
        for layer_idx, layer in enumerate(item["layers"]):
            raw = bytes.fromhex(before[layer_idx])
            if len(raw) != 148:
                raise ValueError("Original VXD descriptor is not 148 bytes")
            source = layer["source"]
            fields = [
                int(case["bank"]), int(case["program"]),
                int(case["key"]), int(case["velocity"]),
                int(source["sample"]),
            ]
            fields += original_preset_template + original_instrument_template
            # Four separate ORIGINAL generator scopes are not equivalent
            # to a single undifferentiated SF2 generator dictionary.
            for name in (
                "raw_preset_global", "raw_preset_local",
                "raw_instrument_global", "raw_instrument_local",
            ):
                scope = [-1] * 64
                for row in layer.get(name, []):
                    op = int(row["operator"])
                    if not 0 <= op < 64:
                        raise ValueError("Original generator ID out of bounds")
                    scope[op] = int(row["amount"]) & 0xFFFF
                fields += scope
            rows.append(" ".join(map(str, fields)))
            expected.append(raw[:118])
            labels.append((case["program"], layer_idx))
    if not rows:
        raise ValueError("No original VXD records to verify")
    return rows, expected, labels


def run() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    parser.add_argument("--private-vxd", type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    handoff = json.loads(args.handoff.read_text(encoding="utf-8"))
    rows, expected, labels = private_rows(handoff, args.private_vxd.read_bytes())
    result = subprocess.run(
        [str(args.executable), "--build"],
        input=str(len(rows)) + "\n" + "\n".join(rows) + "\n",
        text=True, capture_output=True, check=False,
    )
    if result.returncode:
        print(
            "P002_PRIVATE_SF2_BUILDER_FAIL native returned nonzero; "
            f"returncode={result.returncode}",
            file=sys.stderr,
        )
        return 1
    outputs = result.stdout.splitlines()
    if len(outputs) != len(expected):
        print(
            "P002_PRIVATE_SF2_BUILDER_FAIL wrong output count",
            file=sys.stderr,
        )
        return 1
    for label, output, original in zip(labels, outputs, expected):
        try:
            result_bytes = bytes.fromhex(output)
        except ValueError:
            print("P002_PRIVATE_SF2_BUILDER_FAIL bad output hex", file=sys.stderr)
            return 1
        if len(result_bytes) != 118 or result_bytes != original:
            offsets = [
                j for j, (ours, actual) in enumerate(
                    zip(result_bytes, original)
                ) if ours != actual
            ]
            print(
                f"P002_PRIVATE_SF2_BUILDER_FAIL program={label[0]} "
                f"layer={label[1]} unequal_byte_offsets={offsets}",
                file=sys.stderr,
            )
            return 1
    print(
        f"P002_PRIVATE_ORIGINAL_SF2_BUILDER_PASS voices={len(expected)} "
        f"generator_words={59 * len(expected)} verified_by_x86=TRUE "
        "original_pcm_parity=NOT_PROVEN"
    )
    return 0


if __name__ == "__main__":
    sys.exit(run())
