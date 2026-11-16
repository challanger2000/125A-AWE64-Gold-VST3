"""P003 exact original Win95 Creative mode-probe log negative controls."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p003_verify_original_win95_mode_stress_log import (
    validate_original_stress_log as check, PER_BANK)

def record():
    values=[
        "P003_ORIGINAL_WIN95_ALLOCATOR_MODE_STRESS_V1",
        "AWE_SEND_MIDI_OPCODE=0x00000191",
        "PROGRAMS_PER_BANK=0x00000010",
        "CHANNELS_PER_PROGRAM=0x00000003",
        "OVERLAPPING_NOTES_PER_PROGRAM=0x00000024",
        "GUEST_START_TICK_MS=0x00000030",
    ]
    for bank in range(3):
        values.extend([
            f"BANK_PHASE_BEGIN=0x{bank:08X}",
            f"BANK{bank}_ORIGINAL_CREATIVE_MIDI_CALLS=0x{PER_BANK:08X}",
            f"BANK_PHASE_END=0x{bank:08X}"
        ])
    values.extend([
        f"ORIGINAL_MIDI_CALLS_TOTAL=0x{3*PER_BANK:08X}",
        "ORIGINAL_MIDI_CALL_ERRORS=0x00000000",
        "ORIGINAL_BANK_PHASES_COMPLETED=0x00000003",
        "GUEST_END_TICK_MS=0x00001337",
        "P003_ORIGINAL_WIN95_ALLOCATOR_STRESS_COMPLETED"
    ])
    return ("\r\n".join(values)+"\r\n").encode("ascii")

class OriginalCreativeWin95ModeLogTests(unittest.TestCase):
    def test_valid_genuine_creative_midi_submission_does_not_claim_modes(self):
        a=check(record())
        self.assertEqual(a["original_win95_midi_send_calls"],4176)
        self.assertEqual(a["original_midi_calls_by_bank"],
                         {"0":1392,"1":1392,"2":1392})
        self.assertEqual(a["source_driver_mode1_executed"],
                         "NOT_INFERRED_FROM_MIDI_API")
        self.assertTrue(a["original_vxd_guest_callstack_evidence_required"])
    def test_guest_missing_phase_is_no_go(self):
        with self.assertRaisesRegex(ValueError,"Incomplete"):
            check(record().replace(b"BANK_PHASE_BEGIN=0x00000001\r\n",b""))
    def test_guest_wrong_bank_order_is_no_go(self):
        with self.assertRaisesRegex(ValueError,"bank-phase"):
            check(record().replace(b"BANK_PHASE_BEGIN=0x00000001",
                                   b"BANK_PHASE_BEGIN=0x00000002",1))
    def test_creative_midi_failure_is_no_go(self):
        with self.assertRaisesRegex(ValueError,"failed or incomplete"):
            check(record().replace(b"ORIGINAL_MIDI_CALL_ERRORS=0x00000000",
                                   b"ORIGINAL_MIDI_CALL_ERRORS=0x00000001"))
    def test_missing_final_success_marker(self):
        with self.assertRaisesRegex(ValueError,"Incomplete"):
            check(record().replace(
                b"P003_ORIGINAL_WIN95_ALLOCATOR_STRESS_COMPLETED",
                b"P003_ORIGINAL_WIN95_ALLOCATOR_STRESS_NO_GO"))
    def test_missing_one_creative_command_is_no_go(self):
        with self.assertRaisesRegex(ValueError,"wrong MIDI count"):
            check(record().replace(
                b"BANK0_ORIGINAL_CREATIVE_MIDI_CALLS=0x00000570",
                b"BANK0_ORIGINAL_CREATIVE_MIDI_CALLS=0x0000056F"))
    def test_only_exact_original_creative_api_401(self):
        with self.assertRaisesRegex(ValueError,"parameters changed"):
            check(record().replace(b"AWE_SEND_MIDI_OPCODE=0x00000191",
                                   b"AWE_SEND_MIDI_OPCODE=0x00000192"))
    def test_duplicate_or_injected_result_no_go(self):
        with self.assertRaisesRegex(ValueError,"Incomplete"):
            check(record()+b"ORIGINAL_MIDI_CALL_ERRORS=0x00000000\r\n")
    def test_refuse_binary_guest_data(self):
        with self.assertRaisesRegex(ValueError,"binary"):
            check(record()+b"\x00")
    def test_refuse_inconsistent_tick(self):
        with self.assertRaisesRegex(ValueError,"failed or incomplete"):
            check(record().replace(b"GUEST_END_TICK_MS=0x00001337",
                                   b"GUEST_END_TICK_MS=0x00000020"))

if __name__=="__main__":
    unittest.main()
