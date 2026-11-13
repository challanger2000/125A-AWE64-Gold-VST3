"""Synthetic ONLY original Creative RAM C4/D4 note geometry controls.

No real user bank, original driver binary, ROM, captured WAV or private
trace in public CI; the real private trace is audited OFFLINE separately.
"""
from __future__ import annotations
import tempfile
from pathlib import Path
import unittest

from tools import p002_verify_original_ram_note_geometry as checker
from test_p002_win95_ram_guest_log import sf2_bank, synth_guest_log


C4=100000
D4=160000
BASE=0x200010
CCCA=0x23F8CB
PSST=0x23F8D7
CSL=0x23FA68


def source_note(frame,voice):
    out=[]
    def add(reg,port_lo,port_hi,value):
        out.extend((f"{frame} E22 {reg | voice:04X}",
                    f"{frame} {port_lo:03X} {value&65535:04X}",
                    f"{frame} {port_hi:03X} {value>>16:04X}"))
    add(0xC0,0x620,0x622,0x7F000000|PSST)
    add(0xE0,0x620,0x622,0x7F000000|CSL)
    add(0x00,0xA20,0xA22,CCCA)
    add(0x00,0xA20,0xA22,0x80000000|CCCA)
    return out


def synthetic_trace():
    out=["0 E22 0036", "0 A20 0000", "0 A22 0020",
         "0 E22 003A", "0 A20 0001"]
    for frame,voices in ((C4,(0,3)),(D4,(6,9))):
        for j,voice in enumerate(voices):
            out.extend(source_note(frame+j,voice))
    out.append(f"{D4+15} E22 0030")
    return out


class OriginalRamNoteGeometryProofTests(unittest.TestCase):
    def setUp(self):
        self.trace=synthetic_trace()

    def test_complete_two_original_notes_four_source_ram_voices(self):
        report=checker.inspect_note_geometry(self.trace,C4,D4,BASE)
        self.assertEqual(report["status"],
                         "ORIGINAL_CREATIVE_C4_D4_VERIFIED_REAL_RAM_NOT_ROM")
        self.assertEqual(report["original_chip_register_event_count"],
                         len(self.trace))
        self.assertTrue(report["full_original_CCCA_PSST_CSL_records_for_all_four_voices"])
        self.assertFalse(report["production_approved"])
        for name,voices in (
                ("winmm_C4",(0,3)),("creative_AWE_SEND_MIDI_D4",(6,9))):
            self.assertEqual(set(report["original_ram_voice_groups"][name]),
                             {str(v) for v in voices})
            for v in voices:
                data=report["original_ram_voice_groups"][name][str(v)]
                self.assertEqual(data["physical_CCCA_hex"],"0x23f8cb")
                self.assertEqual(data["physical_PSST_hex"],"0x23f8d7")
                self.assertEqual(data["physical_CSL_hex"],"0x23fa68")
                self.assertTrue(data["inside_original_Creative_RAM_bank"])

    def test_old_rom_only_source_fails_even_with_successful_api(self):
        # OLD captured original Win95 source: CCCA=0xFEB5 ROM, other
        # PSST/CSL addresses 0x0152BA / 0x0152F9; reject both routes.
        bad=[line.replace("A22 8023","A22 8000").replace(
                  "A22 0023","A22 0000")
             for line in self.trace]
        with self.assertRaisesRegex(ValueError,"played ROM"):
            checker.inspect_note_geometry(bad,C4,D4,BASE)

    def test_absent_voice_or_incomplete_registers_rejected(self):
        missing=[line for line in self.trace
                 if not (line.startswith(str(D4+1)) and "E22 00C9" in line)]
        # E22 PSST is C9 (6<<5=0xc0, voice9=0xc9).
        with self.assertRaisesRegex(ValueError,"missing source"):
            checker.inspect_note_geometry(missing,C4,D4,BASE)
        incomplete=[line for line in self.trace
                    if line!=f"{C4} 622 7F23"]
        with self.assertRaisesRegex(ValueError,"missing source"):
            checker.inspect_note_geometry(incomplete,C4,D4,BASE)

    def test_wrong_real_source_bank_geometry_or_clock_rejected(self):
        with self.assertRaisesRegex(ValueError,"not inside"):
            checker.inspect_note_geometry(self.trace,C4,D4,0x300010)
        with self.assertRaisesRegex(ValueError,"missing source"):
            checker.inspect_note_geometry(self.trace,C4+300,D4,BASE)
        with self.assertRaisesRegex(ValueError,"timing invalid"):
            checker.inspect_note_geometry(self.trace,D4,C4,BASE)

    def test_original_win95_guest_log_and_sf2_provenance_combined(self):
        with tempfile.TemporaryDirectory(prefix="synthetic-ram-voice-") as td:
            root=Path(td)
            sf2=sf2_bank()
            log=synth_guest_log(len(sf2)).decode("ascii")
            log=log.replace(
                "PRE_LOAD_FREE_16BIT_WORDS=0x00D00000",
                "PRE_LOAD_FREE_16BIT_WORDS=0x00E00000").replace(
                "POST_LOAD_FREE_16BIT_WORDS=0x00CF0000",
                "POST_LOAD_FREE_16BIT_WORDS=0x00DC04B7").replace(
                "CREATIVE_DRIVER_ALLOCATED_WORD_DELTA=0x00010000",
                "CREATIVE_DRIVER_ALLOCATED_WORD_DELTA=0x0003FB49")
            (root/"source.sf2").write_bytes(sf2)
            # synth_guest_log supplies canonical CRLF bytes. write_text()
            # translates LF again on Windows and creates CR-CR-LF, making
            # the independent original-guest auditor reject our own test.
            (root/"guest.log").write_bytes(log.encode("ascii"))
            (root/"trace.log").write_text(
                "\n".join(self.trace)+"\n",encoding="ascii")
            proof=checker.analyze(
                root/"trace.log",root/"guest.log",root/"source.sf2",
                C4,D4,BASE)
            self.assertTrue(
                proof["original_Win95_Creative_API_load_and_MIDI_CC0_1_audited"])
            self.assertEqual(
                proof["status"],"ORIGINAL_CREATIVE_C4_D4_VERIFIED_REAL_RAM_NOT_ROM")
            (root/"guest.log").write_bytes(
                log.replace(
                    "ORIGINAL_BANK_SELECT_MSB1_RC=0x00000000",
                    "ORIGINAL_BANK_SELECT_MSB1_RC=0x00000001").encode("ascii"))
            with self.assertRaisesRegex(ValueError,"BANK_SELECT_MSB1"):
                checker.analyze(root/"trace.log",root/"guest.log",
                                root/"source.sf2",C4,D4,BASE)


if __name__=="__main__":
    unittest.main()
