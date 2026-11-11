import importlib.util
from pathlib import Path
import sys
import unittest
P=Path(__file__).resolve().parent.parent/"tools/p002_voice_register_audit.py"
# The production CLI resolves its own sibling module naturally, but this
# test loads it through importlib from the repo root. Restore that exact
# tools/ package search path instead of silently skipping the integration.
sys.path.insert(0,str(P.parent))
s=importlib.util.spec_from_file_location("emu8k_semantics",P)
mod=importlib.util.module_from_spec(s)
s.loader.exec_module(mod)

class RegisterSemantics(unittest.TestCase):
    def test_original_manual_dcysusv_release_not_engine_disable(self):
        r=mod.interpret("DCYSUSV",0x805c,True)
        self.assertTrue(r["release_phase"])
        self.assertFalse(r["envelope_engine_disabled"])
        self.assertEqual(r["decay_release_7bit"],0x5c)
        off=mod.interpret("DCYSUSV",0x8080,True)
        self.assertTrue(off["release_phase"])
        self.assertTrue(off["envelope_engine_disabled"])
    def test_manual_ifatn_is_filter_high_attenuation_low(self):
        self.assertEqual(mod.interpret("IFATN",0xC42A,True),
            {"filter_cutoff_8bit":0xC4,"attenuation_8bit":0x2A})
    def test_manual_ip_octave_and_center(self):
        self.assertEqual(mod.interpret("IP",0xE000,True)["raw_pitch_offset"],0)
        self.assertEqual(mod.interpret("IP",0xF000,True)["nominal_semitones_from_E000"],12)
    def test_pointer_aware_same_frame_driver_registers(self):
        # E22 pointer = voice 29 + register 1 -> IFATN at DATA3.
        seq=[(11,"WRITE",0xe22,(1<<5)|29),(11,"WRITE",0xe20,0xC42A)]
        r=mod.decode(seq)
        self.assertEqual(r["decoded_writes"],1)
        self.assertEqual(r["writes"][0]["voice"],29)
        self.assertEqual(r["writes"][0]["name"],"IFATN")
        self.assertEqual(r["writes"][0]["filter_cutoff_8bit"],0xC4)
    def test_reverb_and_pan_sends_after_both_16bit_halves(self):
        v=4
        seq=[
        (0,"WRITE",0xe22,(1<<5)|v), # PTRX
        (0,"WRITE",0x620,0x7A0F),  # low16 -> reverb 0x7A
        (0,"WRITE",0x622,0x4000),  # high16 -> pitch target
        (1,"WRITE",0xe22,(6<<5)|v), # PSST
        (1,"WRITE",0x620,0x1234),
        (1,"WRITE",0x622,0x80AB)]
        r=mod.decode(seq)["writes"]
        self.assertNotIn("reverb_send_8bit",r[0])
        self.assertEqual(r[1]["reverb_send_8bit"],0x7a)
        self.assertEqual(r[1]["pitch_target_16bit"],0x4000)
        self.assertEqual(r[3]["pan_8bit"],0x80)
        self.assertEqual(r[3]["loop_start_24bit"],0xAB1234)
    def test_original_ccca_resonance_32bit_data1_pair(self):
        rows=mod.decode([(8,"WRITE",0xe22,4),
                         (8,"WRITE",0xa20,0x3456),
                         (8,"WRITE",0xa22,0xA012)])["writes"]
        self.assertEqual(len(rows),2)
        self.assertFalse(rows[0]["complete_32bit"])
        self.assertEqual(rows[1]["name"],"CCCA")
        self.assertTrue(rows[1]["complete_32bit"])
        self.assertEqual(rows[1]["filter_resonance_Q_4bit"],10)
        self.assertEqual(rows[1]["current_sample_address_24bit"],0x123456)

    def test_original_data2_envelope_is_independent_word(self):
        rows=mod.decode([(0,"WRITE",0xe22,(4<<5)|8),
                         (0,"WRITE",0xa22,0x7F7F)])["writes"]
        self.assertEqual(rows[-1]["name"],"ATKHLDV")

    def test_original_global_data1_32bit_ram_address(self):
        rows=mod.decode([(0,"WRITE",0xe22,(1<<5)|22),
                         (0,"WRITE",0xa20,0x5678),
                         (0,"WRITE",0xa22,0x1234)])["writes"]
        self.assertEqual(rows[-1]["name"],"SMALW")
        self.assertTrue(rows[-1]["complete_32bit"])

    def test_unpaired_32bit_does_not_mint_fake_reverb(self):
        r=mod.decode([(0,"WRITE",0xe22,(1<<5)|2),
                      (0,"WRITE",0x620,0x8000)])
        self.assertEqual(r["decoded_writes"],1)
        self.assertFalse(r["writes"][0]["complete_32bit"])
        self.assertNotIn("reverb_send_8bit",r["writes"][0])
    def test_read_is_not_write_or_pointer(self):
        r=mod.decode([(0,"WRITE",0xe22,0x21),
                      (0,"READ",0xe22,0x5b),
                      (0,"WRITE",0xe20,0xff11)])
        self.assertEqual(r["read_events"],1)
        self.assertEqual(r["writes"][0]["name"],"IFATN")
    def test_pan_is_not_midi_channel(self):
        r=mod.decode([(0,"WRITE",0xe22,(6<<5)|29),
                      (0,"WRITE",0x620,2),
                      (0,"WRITE",0x622,0xf000)])
        self.assertEqual(r["writes"][-1]["voice"],29)
        self.assertEqual(r["writes"][-1]["pan_8bit"],0xf0)
    def test_detects_global_register_vs_voice(self):
        r=mod.decode([(0,"WRITE",0xe22,(1<<5)|31),
                      (0,"WRITE",0xa20,4)])
        self.assertEqual(r["writes"][-1]["name"],"HWCF3")
    def test_wrong_pointer_not_silently_classified(self):
        r=mod.decode([(0,"WRITE",0xe20,123)])
        self.assertEqual(r["unclassified_writes"],1)
        self.assertEqual(r["decoded_writes"],0)

if __name__=="__main__":unittest.main()
