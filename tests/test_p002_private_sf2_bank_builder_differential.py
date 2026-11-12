"""Fail-closed synthetic tests for private original SF2 59-WORD builder oracle.

Never contains Creative driver, ROM or genuine bank data.
"""
import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import p002_private_sf2_bank_builder_differential as runner


class PrivateOriginalSf2BuilderContract(unittest.TestCase):
    def fixture(self):
        before = "00" * 148
        handoff = {
            "schema": "125a-original-bank-to-register-handoff-v1",
            "original_vxd_sha256": runner.EXPECTED_VXD_SHA256,
            "original_bank_sha256":
                "78f8739a25d49e519b3fdc92a7f03d4ddd68588fd0260578a880296e304b7b5f",
            "cases": [{
                "case": {
                    "bank": 0, "program": 0, "key": 60, "velocity": 90,
                    "before": [before],
                },
                "layers": [{
                    "source": {"sample": 3},
                    "raw_preset_local": [],
                    "raw_instrument_global": [],
                    "raw_instrument_local": [{"operator": 53, "amount": 3}],
                }],
            }],
        }
        return handoff

    def test_source_scope_and_template_input_lengths(self):
        handoff = self.fixture()
        with patch.object(runner, "extract", return_value={
            "template_first_59_signed_words": [0] * 59,
        }):
            rows, original, labels = runner.private_rows(handoff, b"synthetic")
        self.assertEqual(len(rows), 1)
        self.assertEqual(original, [bytes(118)])
        self.assertEqual(labels, [(0, 0)])
        values = list(map(int, rows[0].split()))
        # 5 metadata + two 59-WORD templates + four 64-generator scopes.
        self.assertEqual(len(values), 5 + 118 + 256)
        self.assertEqual(values[:5], [0, 0, 60, 90, 3])
        self.assertEqual(values[-64 + 53], 3)  # real raw sampleId

    def test_mutated_original_driver_digest_is_rejected(self):
        handoff = self.fixture()
        handoff["original_vxd_sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "provenance"):
            runner.private_rows(handoff, b"synthetic")

    def test_missing_original_prestage_descriptor_fails(self):
        handoff = self.fixture()
        handoff["cases"][0]["case"]["before"] = []
        with self.assertRaisesRegex(ValueError, "Missing"):
            runner.private_rows(handoff, b"synthetic")

    def test_unknown_original_generator_fails_closed(self):
        handoff = self.fixture()
        handoff["cases"][0]["layers"][0]["raw_instrument_local"].append(
            {"operator": 64, "amount": 1}
        )
        with patch.object(runner, "extract", return_value={
            "template_first_59_signed_words": [0] * 59,
        }):
            with self.assertRaisesRegex(ValueError, "out of bounds"):
                runner.private_rows(handoff, b"synthetic")

    def test_actual_cpp_mismatch_is_rejected(self):
        handoff = self.fixture()
        with tempfile.TemporaryDirectory() as d:
            h = Path(d) / "synthetic-handoff.json"
            v = Path(d) / "synthetic-le.vxd"
            h.write_text(json.dumps(handoff), encoding="utf-8")
            v.write_bytes(b"synthetic")
            args = [
                "p002-private-builder", "--handoff", str(h),
                "--private-vxd", str(v), "--executable",
                str(Path(d) / "mock-native-builder"),
            ]
            with patch.object(sys, "argv", args), patch.object(
                runner, "extract", return_value={
                    "template_first_59_signed_words": [0] * 59,
                }
            ), patch.object(
                runner.subprocess, "run",
                return_value=SimpleNamespace(
                    returncode=0,
                    stdout="01" + "00" * 117 + "\n",
                    stderr="",
                )
            ):
                self.assertEqual(runner.run(), 1)


if __name__ == "__main__":
    unittest.main()
