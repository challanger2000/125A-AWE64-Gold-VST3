#!/usr/bin/env python3
"""Private full 148-byte Creative SF2 x86 differential without publishing bank data."""
import argparse
import json
import subprocess
import sys
from pathlib import Path


def run() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--handoff", type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    handoff = json.loads(args.handoff.read_text(encoding="utf-8"))
    if handoff.get("schema") != "125a-original-bank-to-register-handoff-v1":
        raise ValueError("Unexpected original bank handoff schema")
    if handoff.get("original_vxd_sha256") != (
        "d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a"
    ):
        raise ValueError("Unexpected original SBAWE.VXD provenance")
    if handoff.get("original_bank_sha256") != (
        "78f8739a25d49e519b3fdc92a7f03d4ddd68588fd0260578a880296e304b7b5f"
    ):
        raise ValueError("Unexpected original SYNTHGM.SF2 provenance")
    inputs: list[str] = []
    expected: list[bytes] = []
    names: list[str] = []
    for item in handoff["cases"]:
        case = item["case"]
        state = case["state"]
        before = case["before"]
        after = case["after"]
        if len(before) != len(after) or len(before) != len(item["layers"]):
            raise ValueError("Handoff layer count inconsistent")
        for layer, (source, target) in enumerate(zip(before, after)):
            raw = bytes.fromhex(source)
            oracle = bytes.fromhex(target)
            if len(raw) != 148 or len(oracle) != 148:
                raise ValueError("Original descriptor must be 148 bytes")
            fields = (
                case["bank"], case["key"], case["velocity"],
                state["cc7"], state["cc11"], state["pan"],
                state["chorus"], state["reverb"],
                state["device_attenuation"], state["effects_flag"],
            )
            inputs.append(source + " " + " ".join(map(str, fields)))
            expected.append(oracle)
            names.append(f"program={case['program']} layer={layer}")
    if not expected:
        raise ValueError("Handoff contains no actual descriptors")
    result = subprocess.run(
        [str(args.executable)],
        input="\n".join(inputs) + "\n",
        text=True, capture_output=True, check=False,
    )
    if result.returncode:
        print("P002_ORIGINAL_SF2_PRIVATE_DIFFERENTIAL_FAIL: executable rejected input",
              result.stderr, file=sys.stderr)
        return result.returncode
    outputs = result.stdout.splitlines()
    if len(outputs) != len(expected):
        raise ValueError("Original descriptor output count mismatch")
    for label, line, oracle in zip(names, outputs, expected):
        actual = bytes.fromhex(line)
        if len(actual) != 148:
            raise ValueError(f"{label}: invalid output length")
        if actual != oracle:
            offsets = [
                i for i, (got, exp) in enumerate(zip(actual, oracle))
                if got != exp
            ]
            print(f"P002_ORIGINAL_SF2_PRIVATE_DIFFERENTIAL_FAIL {label} "
                  f"different_byte_offsets={offsets}", file=sys.stderr)
            return 1
    print(f"P002_ORIGINAL_SF2_PRIVATE_148BYTE_PASS voices={len(expected)} "
          "comparison=full_binary_equal driver_pcm=NOT_PROVEN")
    return 0


if __name__ == "__main__":
    sys.exit(run())
