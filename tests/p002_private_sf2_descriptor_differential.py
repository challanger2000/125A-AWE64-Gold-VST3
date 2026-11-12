#!/usr/bin/env python3
"""Private full 148-byte + original DATA3 IP Creative SF2 differential."""
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
    expected_ips: list[int] = []
    names: list[str] = []
    for item in handoff["cases"]:
        case = item["case"]
        state = case["state"]
        before = case["before"]
        after = case["after"]
        if (len(before) != len(after) or len(before) != len(item["layers"])
                or len(before) != len(case["physical_voices"])):
            raise ValueError("Handoff layer / original physical voice count inconsistent")
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
            physical = case["physical_voices"][layer]
            # The actual VXD writes DATA3/IP at E22 selection byte =
            # assigned physical voice, followed by E20 16-bit value.
            # Do not assume the declared numerical IP in a report is the
            # register oracle: derive it from this ORIGINAL typed port trace.
            ports = case["ports"]
            actual_ip_writes = [
                following[2]
                for prior, following in zip(ports, ports[1:])
                if prior == [0xE22, 1, physical]
                and following[0] == 0xE20 and following[1] == 2
            ]
            if len(actual_ip_writes) != 1:
                raise ValueError(
                    f"program={case['program']} layer={layer}: "
                    f"expected exactly one original DATA3 IP write, "
                    f"found {len(actual_ip_writes)}"
                )
            expected_ips.append(actual_ip_writes[0])
            names.append(f"program={case['program']} layer={layer}")
    if not expected:
        raise ValueError("Handoff contains no actual descriptors")
    result = subprocess.run(
        [str(args.executable), "--also-ip"],
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
    for label, line, oracle, original_ip in zip(
            names, outputs, expected, expected_ips):
        parts = line.split(" ")
        if len(parts) != 2 or len(parts[1]) != 4:
            raise ValueError(f"{label}: expected descriptor+IP fields")
        actual = bytes.fromhex(parts[0])
        encoded_ip = int(parts[1], 16)
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
        if encoded_ip != original_ip:
            print(f"P002_ORIGINAL_SF2_PRIVATE_IP_FAIL {label} "
                  f"original={original_ip:04x} candidate={encoded_ip:04x}",
                  file=sys.stderr)
            return 1
    print(f"P002_ORIGINAL_SF2_PRIVATE_148BYTE_AND_IP_PASS voices={len(expected)} "
          "descriptor_bytes=equal original_data3_ip=equal "
          "driver_pcm=NOT_PROVEN")
    return 0


if __name__ == "__main__":
    sys.exit(run())
