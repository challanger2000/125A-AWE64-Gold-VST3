#!/usr/bin/env python3
"""Negative-only contract for genuine PRIVATE Win95 Creative audio oracle.

NO fake original 86Box/Win95 test file is presented as an audio PASS.
"""
from __future__ import annotations
import importlib.util
import tempfile
from pathlib import Path
import unittest
import wave

GATE=Path(__file__).resolve().parent.parent/"tools/p001_verify_independent_win95_midi.py"
spec=importlib.util.spec_from_file_location("private_86box_gate",GATE)
gate=importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(gate)


def valid_log():
    lines=[
        "P001 ORIGINAL CREATIVE WIN95 MIDI PROBE ENTRYPOINT",
        "WINMM device count=0x00000002",
        "MIDI_DEVICE 0x00000000 name=SB AWE32 MIDI Synth [620]",
        "AWE MIDI OPEN result=0x00000000",
        "ORIGINAL_CREATIVE_AWE_WINMM_SELECTED",
        "RESET_ALL_CONTROLLERS value=0x000079B0 result=0x00000000",
        "PROGRAM_0_ROM_GM value=0x000000C0 result=0x00000000",
        "CC7_VOLUME value=0x007F07B0 result=0x00000000",
        "CC11_EXPRESSION value=0x007F0BB0 result=0x00000000",
    ]
    for pitch in gate.PITCHES:
        lines.append(f"NOTE_ON value=0x{(0x90|(pitch<<8)|(100<<16)):08X} result=0x00000000")
        lines.append(f"NOTE_OFF value=0x{(0x80|(pitch<<8)):08X} result=0x00000000")
    return "\n".join(lines)+"\n"


class OriginalWin95NegativeControls(unittest.TestCase):
    def setUp(self):
        self.d=tempfile.TemporaryDirectory()
        self.addCleanup(self.d.cleanup)
        self.root=Path(self.d.name)

    def log(self,text):
        p=self.root/"guest.log"
        p.write_text(text,encoding="latin-1")
        return p

    def test_valid_complete_genuine_guest_log_structure(self):
        # This tests only the strict parser, NOT real driver execution.
        self.assertEqual(gate.guest_check(self.log(valid_log())),5)

    def test_missing_noteoff_is_rejected(self):
        contents=valid_log()
        contents=contents.replace("NOTE_OFF value=0x00004880 result=0x00000000\n","")
        with self.assertRaisesRegex(ValueError,"five successful"):
            gate.guest_check(self.log(contents))

    def test_failed_midi_message_is_rejected(self):
        contents=valid_log().replace(
            "NOTE_ON value=0x00643090 result=0x00000000",
            "NOTE_ON value=0x00643090 result=0x00000001")
        with self.assertRaisesRegex(ValueError,"WinMM error"):
            gate.guest_check(self.log(contents))

    def test_wrong_instrument_is_rejected(self):
        contents=valid_log().replace("SB AWE32 MIDI Synth [620]","Microsoft MIDI Mapper")
        with self.assertRaisesRegex(ValueError,"not proven"):
            gate.guest_check(self.log(contents))

    def test_silent_reference_hard_fails(self):
        p=self.root/"silent.wav"
        with wave.open(str(p),"wb") as w:
            w.setparams((2,2,44100,0,"NONE","not compressed"))
            w.writeframes(b"\x00\x00\x00\x00"*441000)
        with self.assertRaisesRegex(ValueError,"effectively silent"):
            gate.audio_check(p)

    def test_invalid_trace_clock_fails(self):
        p=self.root/"bad.trace"
        p.write_text("# source independent fork\n10 E22 00E0\n9 E22 00A0\n",encoding="ascii")
        with self.assertRaisesRegex(ValueError,"Invalid VM register clock"):
            gate.trace_check(p,5000,0,5000)

    def test_missing_register_activity_fails(self):
        p=self.root/"boot-only.trace"
        p.write_text("# initialized only\n5 E22 00E0\n",encoding="ascii")
        with self.assertRaisesRegex(ValueError,"No meaningful original Creative port activity"):
            gate.trace_check(p,5000,0,5000)

    def test_wrong_owner_rom_sha_fails(self):
        p=self.root/"wrong.raw"
        p.write_bytes(b"\x00"*1048576)
        self.assertNotEqual(gate.digest(p),gate.ROM_SHA)


if __name__=="__main__":
    unittest.main()
