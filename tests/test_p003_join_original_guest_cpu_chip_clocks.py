"""P003: no private data; synthetic 86Box CPU/chip clock-join controls."""
import io
import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from p003_join_original_guest_cpu_chip_clocks import (
    chip_events, guest_events, note_fixture, join_traces)

CHIP = """# EMU8000 port write trace, 44100 Hz timebase
# synthetic
42 E22 0060
R 42 A20 1234
43 620 FFFF
44 E22 00A0
50 E22 0040
54 620 8010
"""
CPU = """# P003_CPU_GUEST_PHYSICAL16_V2 synthetic
W 10000 200 1234 ABCDEF00 E22 0060 0
R 10012 185 1234 ABCDEF02 A20 1234 1
W 10028 170 1234 ABCDEF04 620 FFFF 2
W 11020 160 1234 ABCDEF10 E22 00A0 3
W 15000 200 1234 ABCDEF14 E22 0040 4
W 16300 145 1234 ABCDEF1A 620 8010 5
"""
NOTES = """# synthetic note groups
# C4_ON
42 E22 0060
43 620 FFFF
# C4_OFF
44 E22 00A0
# D4_ON
50 E22 0040
# D4_OFF
54 620 8010
"""

def compare(chip=CHIP,cpu=CPU,notes=NOTES):
    return join_traces(io.StringIO(chip),io.StringIO(cpu),
                       note_fixture(io.StringIO(notes)) if notes else None)

class TestOriginalChipCpuClockJoin(unittest.TestCase):
    def test_all_original_io_events_must_match(self):
        result=compare()
        self.assertEqual(result["matched_io_events"],6)
        self.assertEqual(result["physical_write_events"],5)
        self.assertTrue(result["event_sequence_exact"])
        self.assertFalse(result["guest_cpu_to_chip_clock_calibrated"])
        self.assertEqual(result["note_groups"]["C4_ON"]["guest_tsc_delta"],28)
        self.assertEqual(result["note_groups"]["D4_ON"]["start_guest_tsc"],15000)
        self.assertEqual(result["note_groups"]["C4_ON"]["guest_x86_call_sites"],
                         {"1234:ABCDEF00":1,"1234:ABCDEF04":1})
    def test_wrong_port_value_must_fail(self):
        with self.assertRaisesRegex(ValueError,"mismatch at event 2"):
            compare(cpu=CPU.replace("620 FFFF 2","620 FFFE 2"))
    def test_missing_event_fails(self):
        with self.assertRaisesRegex(ValueError,"count differs"):
            compare(cpu=CPU.replace("W 16300 145 1234 ABCDEF1A 620 8010 5\n",""))
    def test_reordered_cpu_seq_fails(self):
        with self.assertRaisesRegex(ValueError,"out of order"):
            compare(cpu=CPU.replace("620 FFFF 2","620 FFFF 8"))
    def test_reordered_original_chip_clock_fails(self):
        with self.assertRaisesRegex(ValueError,"order/port/value"):
            compare(chip=CHIP.replace("43 620 FFFF","41 620 FFFF"))
    def test_corrupted_note_reference_fails(self):
        with self.assertRaisesRegex(ValueError,"did not match"):
            compare(notes=NOTES.replace("43 620 FFFF","43 620 FFFE"))
    def test_guest_tsc_backwards_is_reported_not_hidden(self):
        r=compare(cpu=CPU.replace("W 11020 160","W 9000 160"))
        self.assertEqual(r["guest_tsc_regressions"],1)
        self.assertFalse(r["guest_cpu_to_chip_clock_calibrated"])
    def test_no_original_note_fixture_allowed_without_note_claim(self):
        r=compare(notes=None)
        self.assertEqual(r["note_groups"],{})
    def test_reject_bogus_original_headers(self):
        with self.assertRaisesRegex(ValueError,"header"):
            compare(chip=CHIP.replace("EMU8000","BOGUS"))
        with self.assertRaisesRegex(ValueError,"header"):
            compare(cpu=CPU.replace("P003_CPU_GUEST_PHYSICAL16_V2","BOGUS"))
    def test_old_16bit_only_guest_probe_is_rejected(self):
        old = CPU.replace("P003_CPU_GUEST_PHYSICAL16_V2",
                          "P003_CPU_GUEST_OUTW_V1")
        with self.assertRaisesRegex(ValueError,
                                    "omits physical EMU8000 callbacks"):
            compare(cpu=old)

if __name__=="__main__":
    unittest.main()
