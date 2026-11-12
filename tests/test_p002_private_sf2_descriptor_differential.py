"""Synthetic tests of private original-driver descriptor+IP differential runner.

No private Creative ROM, driver or SF2 bytes are published by this fixture.
"""
import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import p002_private_sf2_descriptor_differential as runner


class PrivateSf2PitchDifferentialContractTest(unittest.TestCase):
    def _fixture(self):
        value = "00" * 148
        data = {
            "schema": "125a-original-bank-to-register-handoff-v1",
            "original_vxd_sha256": (
                "d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a"
            ),
            "original_bank_sha256": (
                "78f8739a25d49e519b3fdc92a7f03d4ddd68588fd0260578a880296e304b7b5f"
            ),
            "cases": [
                {
                    "case": {
                        "bank": 0, "program": 7, "key": 60, "velocity": 90,
                        "state": {
                            "cc7": 100, "cc11": 127, "pan": 64,
                            "chorus": 0, "reverb": 0,
                            "device_attenuation": 0, "effects_flag": 0,
                        },
                        "before": [value],
                        "after": [value],
                        "physical_voices": [3],
                        "ports": [
                            [0xE22, 1, 3],
                            [0xE20, 2, 0xB2B5],
                        ],
                    },
                    "layers": [{}],
                }
            ],
        }
        return data, value

    def _run(self, data, stdout):
        with tempfile.TemporaryDirectory() as d:
            handoff = Path(d) / "synthetic.json"
            handoff.write_text(json.dumps(data), encoding="utf-8")
            argv = [
                "p002-private-differential", "--handoff", str(handoff),
                "--executable", str(Path(d) / "synthetic-mock-executable"),
            ]
            fake = SimpleNamespace(
                returncode=0, stdout=stdout, stderr=""
            )
            with patch.object(sys, "argv", argv), patch.object(
                runner.subprocess, "run", return_value=fake
            ) as call:
                output = runner.run()
                if call.called:
                    self.assertEqual(call.call_args.args[0][-1], "--also-ip")
                return output

    def test_original_port_matches_descriptor_and_ip(self):
        data, descriptor = self._fixture()
        self.assertEqual(self._run(data, descriptor + " b2b5\n"), 0)

    def test_changed_original_ip_must_fail(self):
        data, descriptor = self._fixture()
        self.assertEqual(self._run(data, descriptor + " b2b4\n"), 1)

    def test_changed_descriptor_byte_must_fail(self):
        data, descriptor = self._fixture()
        changed = "01" + descriptor[2:]
        self.assertEqual(self._run(data, changed + " b2b5\n"), 1)

    def test_missing_original_ip_port_must_fail(self):
        data, descriptor = self._fixture()
        data["cases"][0]["case"]["ports"] = [[0xE22, 1, 3]]
        with self.assertRaises(ValueError):
            self._run(data, descriptor + " b2b5\n")


if __name__ == "__main__":
    unittest.main()
