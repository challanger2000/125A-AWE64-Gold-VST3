"""Synthetic, fail-closed tests of the private original-table rate matrix.

No actual Creative driver/tables/bank material is committed to CI.
"""
import struct
import unittest
from types import SimpleNamespace
from unittest.mock import patch

import p002_private_sf2_pitch_matrix_differential as mod


class OriginalSf2PitchMatrixContract(unittest.TestCase):
    @staticmethod
    def synthetic():
        # Artificial base/slope lookups; NEVER original Creative values.
        return {
            "pitch_table_a_unsigned_32": [17 + 100 * i for i in range(128)],
            "pitch_table_b_signed_8": [i // 2 - 32 for i in range(128)],
        }

    def test_reference_zero_rate_fails_closed(self):
        with self.assertRaises(ValueError):
            mod.original_reference(0, [0] * 128, [0] * 128)

    def test_reference_integer_wrap_and_nonzero_signed_slopes(self):
        p = self.synthetic()
        a = mod.original_reference(
            48000, p["pitch_table_a_unsigned_32"],
            p["pitch_table_b_signed_8"]
        )
        self.assertTrue(0 <= a <= 0xFFFFFFFF)
        b = p["pitch_table_b_signed_8"].copy()
        b[11] += 1
        # The 48k sample-rate index=11; changing one real table entry must
        # affect its result. This confirms slope signs affect the matrix.
        self.assertEqual(
            mod.original_reference(48000, p["pitch_table_a_unsigned_32"], b)
            - a, 41
        )

    def test_invalid_resource_shapes_rejected(self):
        p = self.synthetic()
        p["pitch_table_a_unsigned_32"].pop()
        with self.assertRaises(ValueError):
            mod.compare_native("mock", p)
        p = self.synthetic()
        with self.assertRaises(ValueError):
            mod.compare_native("mock", p, rates=192001)

    def test_nonzero_native_status_fails(self):
        with patch.object(mod.subprocess, "run", return_value=SimpleNamespace(
            returncode=3, stdout="", stderr="")):
            with self.assertRaisesRegex(ValueError, "rejected"):
                mod.compare_native("mock", self.synthetic())

    def test_truncated_native_rate_matrix_fails(self):
        with patch.object(mod.subprocess, "run", return_value=SimpleNamespace(
            returncode=0, stdout="00000000\n", stderr="")):
            with self.assertRaisesRegex(ValueError, "Incomplete"):
                mod.compare_native("mock", self.synthetic())

    def test_wrong_original_lookup_value_fails(self):
        with patch.object(mod.subprocess, "run", return_value=SimpleNamespace(
            returncode=0, stdout="00000000\n" * 192000, stderr="")):
            with self.assertRaisesRegex(ValueError, "mismatch at sample rate 1"):
                mod.compare_native("mock", self.synthetic())

    def test_entire_synthetic_192000_matrix_passes_with_exact_digest(self):
        p = self.synthetic()
        base = p["pitch_table_a_unsigned_32"]
        slope = p["pitch_table_b_signed_8"]
        expected = [
            mod.original_reference(rate, base, slope)
            for rate in range(1, 192001)
        ]
        stdout = "".join(f"{value:08x}\n" for value in expected)
        with patch.object(mod.subprocess, "run", return_value=SimpleNamespace(
            returncode=0, stdout=stdout, stderr="")) as runner:
            count, digest = mod.compare_native("mock", p)
        self.assertEqual(count, 192000)
        import hashlib
        reference = hashlib.sha256(
            b"".join(struct.pack("<I", v) for v in expected)
        ).hexdigest()
        self.assertEqual(digest, reference)
        self.assertEqual(runner.call_args.args[0][-1], "--pitch-matrix")


if __name__ == "__main__":
    unittest.main()
