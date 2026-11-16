"""Mutation-negative contract for private original SF2 18-OUT differential.

Only synthetic arbitrary port words are present; not the user's original
SBAWE.VXD, ROM, original SF2, or full register trace.
"""
import unittest
import p002_private_sf2_note_start_prefix_differential as mod


def typed_ports():
    # 9 synthetic pointer/data pairs. Confirm ONLY typed structural
    # equivalence and sequence mutation handling.
    result = []
    for reg in range(9):
        result.append((0xe22, 8, 0x20 + reg))
        result.append((0xe20, 16, 0x400 + reg))
    return result


def output_line(ports):
    return " ".join(f"{port:04x}:{bits:02x}:{value:08x}"
                    for port, bits, value in ports)


class ExactOriginalSf2PrefixComparator(unittest.TestCase):
    def test_contiguous_original_subsequence_at_arbitrary_offsets(self):
        expected = typed_ports()
        padding = [(0xe22, 8, 0x61), (0xa20, 16, 0xff)]
        cases = [(100, 0, 0, padding * 5 + expected + padding),
                 (100, 1, 3, padding * 34 + expected)]
        self.assertEqual(
            mod.check_original_ports(output_line(expected) + "\n" +
                                     output_line(expected) + "\n", cases), 2)

    def test_one_value_mutation_fails(self):
        expected = typed_ports()
        corrupted = expected.copy()
        corrupted[17] = (corrupted[17][0], corrupted[17][1], 0x123)
        with self.assertRaisesRegex(ValueError, "matches=0"):
            mod.check_original_ports(
                output_line(corrupted) + "\n", [(0, 0, 0, expected)])

    def test_one_write_reordered_fails(self):
        expected = typed_ports()
        altered = expected.copy()
        altered[12], altered[14] = altered[14], altered[12]
        with self.assertRaisesRegex(ValueError, "matches=0"):
            mod.check_original_ports(
                output_line(altered) + "\n", [(40, 0, 0, expected)])

    def test_duplicate_identical_original_sequence_fails(self):
        expected = typed_ports()
        with self.assertRaisesRegex(ValueError, "matches=2"):
            mod.check_original_ports(
                output_line(expected) + "\n",
                [(0, 0, 0, expected + expected)])

    def test_width_and_size_invalid(self):
        expected = typed_ports()
        corrupt = expected.copy()
        corrupt[1] = (0xe20, 8, 0x400)
        with self.assertRaisesRegex(ValueError, "width/value"):
            mod.check_original_ports(
                output_line(corrupt) + "\n", [(0, 0, 0, expected)])
        with self.assertRaisesRegex(ValueError, "exactly 18"):
            mod.check_original_ports(
                output_line(expected[:-1]) + "\n",
                [(0, 0, 0, expected)])

    def test_sha_handoff_and_voice_count_rejected(self):
        after = bytearray(148)
        after[0x8c] = 1
        data = {
            "schema": "125a-original-bank-to-register-handoff-v1",
            "original_vxd_sha256": mod.EXPECTED_VXD_SHA,
            "original_bank_sha256": mod.EXPECTED_BANK_SHA,
            "cases": [{
                "case": {
                    "key": 60, "program": 0,
                    "after": [after.hex()], "physical_voices": [0],
                    "ports": [[a, b // 8, c] for a, b, c in typed_ports()],
                }
            }],
        }
        rows, cases = mod.rows_from_handoff(data)
        self.assertEqual(len(rows), 1)
        self.assertEqual(len(cases), 1)
        data["original_vxd_sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "digest mismatch"):
            mod.rows_from_handoff(data)
        data["original_vxd_sha256"] = mod.EXPECTED_VXD_SHA
        data["cases"][0]["case"]["physical_voices"] = [0, 3]
        with self.assertRaisesRegex(ValueError, "count mismatch"):
            mod.rows_from_handoff(data)

    def test_reference_must_have_poststart_sample_type(self):
        after = bytearray(148)
        after[0x8c:0x8e] = b"\x01\x80"
        data = {
            "schema": "125a-original-bank-to-register-handoff-v1",
            "original_vxd_sha256": mod.EXPECTED_VXD_SHA,
            "original_bank_sha256": mod.EXPECTED_BANK_SHA,
            "cases": [{
                "case": {"key": 60, "program": 0, "after": [after.hex()],
                         "physical_voices": [0], "ports": []}
            }],
        }
        with self.assertRaisesRegex(ValueError, "Unsupported"):
            mod.rows_from_handoff(data)


if __name__ == "__main__":
    unittest.main()
