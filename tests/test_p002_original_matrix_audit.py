import importlib.util
from pathlib import Path
import sys
import unittest
P=Path(__file__).resolve().parent.parent/"tools/p002_original_matrix_audit.py"
sys.path.insert(0,str(P.parent))
s=importlib.util.spec_from_file_location("p002_real_guest_matrix",P)
m=importlib.util.module_from_spec(s)
s.loader.exec_module(m)

class RealOriginalMatrixFailClosed(unittest.TestCase):
    def note_starts(self,count=13):
        return [dict(frame=5000000+44100*i,voice=i%32,ip=0xe000,ifatn=0xb220)
                for i in range(count)]
    def note_events(self,count=13):
        return [dict(tick_ms=1000*i,name="TEST",raw=0x00903C90)
                for i in range(count)]
    def mock_guest(self):
        lines=[
            "P002_ORIGINAL_CREATIVE_WIN95_MATRIX_START",
            "P002_DEVICE 0x00000000 NAME=SB AWE32 MIDI Synth [620]",
            "P002_CREATIVE_OPEN_RESULT=0x00000000",
            "P002_ORIGINAL_CREATIVE_WINMM_SELECTED",
        ]
        lines.extend(
            f"SCENARIO index=0x{i:08X} tick_ms=0x{i*100:08X} name={v}"
            for i,v in enumerate(m.EXPECTED_SCENARIOS))
        for i in range(62):
            if i<13:
                v=0x90|(60<<8)|((1 if i==0 else 64 if i==1 else 127 if i==2 else 95)<<16)
            elif i==13: v=0x90|(60<<8)
            else: v=0xB0|(7<<8)|(100<<16)
            name=("VELOCITY_1_ON" if i==0 else "VELOCITY_64_ON" if i==1
                  else "VELOCITY_127_ON" if i==2 else
                  "ZERO_VELOCITY_NOTEON_OFF" if i==13 else f"CASE_{i}")
            lines.append(
                f"MIDI seq=0x{i:08X} tick_ms=0x{i*100:08X} name={name} "
                f"value=0x{v:08X} rc=0x00000000")
        lines.append("P002_GUEST_ORIGINAL_WINMM_MATRIX_SENT_OK count=0x0000003E")
        return "\n".join(lines)+"\n"
    def test_positive_log_parser_is_not_driver_verification(self):
        rows,notes,scenes=m.parse_guest(self.mock_guest())
        self.assertEqual((len(rows),len(notes),len(scenes)),(62,13,7))
    def test_guest_missing_terminal_marker_fails(self):
        with self.assertRaisesRegex(ValueError,"termination"):
            m.parse_guest(self.mock_guest().replace(
                "P002_GUEST_ORIGINAL_WINMM_MATRIX_SENT_OK count=0x0000003E",""))
    def test_guest_wrong_device_fails(self):
        with self.assertRaisesRegex(ValueError,"device"):
            m.parse_guest(self.mock_guest().replace(
                "SB AWE32 MIDI Synth [620]","Microsoft MIDI Mapper"))
    def test_guest_one_failed_winmm_message_fails(self):
        with self.assertRaisesRegex(ValueError,"errors"):
            m.parse_guest(self.mock_guest().replace(
                "CASE_14 value=0x006407B0 rc=0x00000000",
                "CASE_14 value=0x006407B0 rc=0x00000001"))
    def test_guest_message_missing_fails(self):
        with self.assertRaisesRegex(ValueError,"incomplete"):
            m.parse_guest(self.mock_guest().replace(
                "MIDI seq=0x0000003D tick_ms=0x000017D4 name=CASE_61 value=0x006407B0 rc=0x00000000\n",""))
    def test_guest_zero_velocity_not_a_real_voice(self):
        rows,notes,_=m.parse_guest(self.mock_guest())
        self.assertEqual(len(notes),13)
        self.assertEqual(rows[13]["name"],"ZERO_VELOCITY_NOTEON_OFF")
    def test_mismatched_voice_starts_cannot_claim_original_mapping(self):
        with self.assertRaisesRegex(ValueError,"correlate"):
            m.clock_fit(self.note_events(13),self.note_starts(12))
    def test_wrong_guest_to_chip_clock_scale_fails(self):
        with self.assertRaisesRegex(ValueError,"do not match"):
            m.clock_fit(self.note_events(), [
                dict(frame=5000000+48000*i) for i in range(13)])
    def test_uncorrelated_original_guest_clock_fails(self):
        shifted=self.note_starts()
        shifted[8]["frame"]+=10000
        with self.assertRaisesRegex(ValueError,"do not match"):
            m.clock_fit(self.note_events(),shifted)
    def test_simple_voice_start_extracts_original_IP_and_IFATN(self):
        rows=[(100,"WRITE",0xE22,(5<<5)|3),
              (100,"WRITE",0xA20,0xFF),
              (100,"WRITE",0xE22,3),
              (100,"WRITE",0xE20,0xC962),
              (100,"WRITE",0xE22,(1<<5)|3),
              (100,"WRITE",0xE20,0xB248)]
        with self.assertRaisesRegex(ValueError,"count"):
            m.voice_starts(rows)
    def test_guest_failure_marker_overrides_apparent_success(self):
        with self.assertRaisesRegex(ValueError,"termination"):
            m.parse_guest(self.mock_guest()+
                          "P002_GUEST_ORIGINAL_WINMM_MATRIX_FAILED\n")

if __name__=="__main__":
    unittest.main()
