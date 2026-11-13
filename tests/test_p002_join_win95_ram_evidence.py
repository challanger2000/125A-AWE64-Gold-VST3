"""Source-only integration test: Win95 FAT16 log + same SF2 + EMU8K_TRACE.

No original Creative driver/ROM/VM user files. Every byte generated
inside the test is synthetic; no native VST3 PCM equivalence claimed.
"""
from __future__ import annotations

from pathlib import Path
import json
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import wave

from tools import p002_join_original_win95_ram_evidence as joined
from tools import p002_stage_original_win95_ram_probe as staged
from test_p002_extract_win95_ram_log_fat16 import (
    new_guest, add_original_guest_log)
from test_p002_win95_ram_guest_log import synth_guest_log
from test_p002_original_ram_trace_probe import (
    sf2_bank, captured)


class OriginalWin95RamCombinedSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(prefix="p002-full-guest-")
        cls.root=Path(cls.temp.name)
        bank,words=sf2_bank()
        cls.bank=bank
        cls.words=words
        cls.original,cls.clone,cls.sf2=new_guest(cls.root,bank)
        cls.log=synth_guest_log(len(bank),bank)
        add_original_guest_log(cls.clone,cls.log)
        cls.trace=cls.root/"private-original-shaped-source.trace"
        cls.trace.write_text(
            "\n".join(captured(words))+"\n",encoding="ascii")
        cls.wav=cls.root/"synthetic-audio.wav"
        # Audibility/RIFF QA fixture, NOT original Creative audio.
        with wave.open(str(cls.wav),"wb") as w:
            w.setnchannels(2)
            w.setsampwidth(2)
            w.setframerate(44100)
            w.writeframes(struct.pack("<hh",320,-320)*44100)
        cls.original_sha=staged.sha256(cls.original)
        cls.clone_sha=staged.sha256(cls.clone)

    @classmethod
    def tearDownClass(cls):
        assert staged.sha256(cls.original)==cls.original_sha
        assert staged.sha256(cls.clone)==cls.clone_sha
        cls.temp.cleanup()

    def test_real_join_logic_guest_log_bank_trace_and_dry_inventory(self):
        log,result=joined.analyze_join(
            self.clone,self.sf2,self.trace,0,self.wav)
        self.assertEqual(log,self.log)
        self.assertEqual(result["captured_source_sf2_physical_RAM_word_base"],
                         0x200400)
        self.assertEqual(result["status"],
            "P002_PRIVATE_GUEST_AND_SOURCE_RAM_CAPTURE_JOIN_CANDIDATE")
        self.assertTrue(result[
            "actual_original_guest_log_fat16_extracted"])
        self.assertEqual(
            result["private_source_sf2_sha256"],
            result["original_emu8000_RAM_source_sample_match"][
                "source_sf2_sha256"])
        self.assertTrue(result[
            "independent_original_dry_PCM_inventory"][
                "original_dry_signal_detected"])
        self.assertEqual(
            result["independent_original_dry_PCM_inventory"][
                "independent_original_driver_VST3_pcm_parity"],
            "NOT_PROVEN")
        self.assertEqual(result["source_bank_load_call_vs_EMU8000_clock_correlation"],
                         "NOT_PROVEN_FROM_INDEPENDENT_CLOCKS")
        self.assertFalse(result["release_approved"])
        self.assertTrue(result[
            "captured_source_sf2_RAM_final_state_no_overlap_candidate"])
        self.assertEqual(
            result["original_emu8000_RAM_source_sample_match"][
                "original_chip_trace_evidence"][
                    "final_state_unoverwritten_pcm16_matches"],1)

    def test_complete_private_cli_source_trace_filter_and_output(self):
        out_log=self.root/"private-joined-original.log"
        out_json=self.root/"private-joined-original.json"
        out_ports=self.root/"private-joined-ram-source.trace"
        with patch.object(sys,"argv",[
            "p002_join_original_win95_ram_evidence.py",
            "--private-booted-win95-clone",str(self.clone),
            "--private-sf2",str(self.sf2),
            "--original-emu8k-trace",str(self.trace),
            "--sample-id","0",
            "--original-dry-wav",str(self.wav),
            "--private-log-output",str(out_log),
            "--private-report",str(out_json),
            "--private-filtered-ram-ports",str(out_ports),
        ]):
            joined.main()
        self.assertEqual(out_log.read_bytes(),self.log)
        report=json.loads(out_json.read_text(encoding="utf-8"))
        self.assertEqual(
            report["captured_source_sf2_physical_RAM_word_base"],0x200400)
        self.assertFalse(report["native_VST3_RAM_SF2_sound_ready"])
        event_lines=[
            x for x in out_ports.read_text().splitlines()
            if not x.startswith("#")]
        self.assertGreater(len(event_lines),40)
        self.assertTrue(any(
            x.endswith(f"0A20 {self.words[0]:04X}")
            for x in event_lines))
        self.assertFalse(any(" E20 " in x for x in event_lines))
        # Reusing an old private result should be refused rather than
        # overwriting source evidence (without executing the full work).
        with patch.object(sys,"argv",[
            "p002_join_original_win95_ram_evidence.py",
            "--private-booted-win95-clone",str(self.clone),
            "--private-sf2",str(self.sf2),
            "--original-emu8k-trace",str(self.trace),
            "--sample-id","0",
            "--private-log-output",str(out_log),
            "--private-report",str(out_json),
        ]):
            with self.assertRaises(SystemExit):
                joined.main()

    def test_mutated_sample_or_source_file_identity_fails_closed(self):
        lines=self.trace.read_text(encoding="ascii").splitlines()
        index=next(
            i for i,x in enumerate(lines)
            if x.endswith(f"A20 {self.words[0]:04X}"))
        lines[index]=(lines[index][:-4] +
                      f"{self.words[0]^1:04X}")
        wrongtrace=self.root/"private-bad-trace.trace"
        wrongtrace.write_text("\n".join(lines)+"\n",encoding="ascii")
        with self.assertRaisesRegex(ValueError,"EXACTLY ONE"):
            joined.analyze_join(self.clone,self.sf2,wrongtrace,0)
        bad=bytearray(self.bank)
        bad[-8]^=1
        wrongbank=self.root/"private-different-sf2.sf2"
        wrongbank.write_bytes(bad)
        with self.assertRaisesRegex(ValueError,"differs"):
            joined.analyze_join(self.clone,wrongbank,self.trace,0)

    def test_duplicate_physical_original_ram_sample_addresses_no_go(self):
        other=[
            "45 E22 0036",
            "45 A20 0800",
            "45 A22 0020",
            "46 E22 003A",
        ] + [
            f"{46+i} A20 {word:04X}"
            for i,word in enumerate(self.words)]
        wrong=self.root/"private-ambiguous-original-address.trace"
        wrong.write_text(
            self.trace.read_text(encoding="ascii") +
            "\n".join(other)+"\n",encoding="ascii")
        with self.assertRaisesRegex(ValueError,"EXACTLY ONE"):
            joined.analyze_join(self.clone,self.sf2,wrong,0)

    def test_codex_overwritten_historical_ram_source_rejected_by_join(self):
        # Counterexample #1: complete original-looking upload, then
        # source VM overwrites first sample word through another
        # perfectly formed SMALW/SMLD sequence.
        tail=[
            "50 E22 0036","50 A20 0400","50 A22 0020",
            "50 E22 003A","50 A20 DEAD",
        ]
        mutated=self.root/"private-overwritten-historical-source.trace"
        mutated.write_text(
            self.trace.read_text(encoding="ascii") +
            "\n".join(tail)+"\n",encoding="ascii")
        with self.assertRaisesRegex(ValueError,"overwritten|overlapped"):
            joined.analyze_join(self.clone,self.sf2,mutated,0)

    def test_codex_partial_final_pointer_rejected_by_join(self):
        # Counterexample #2: Source capture ends with an uncommitted
        # SMALW low-address half. A completed earlier sample cannot
        # authorize this incomplete Win95 hardware trace.
        partial=self.root/"private-partial-final-smalw.trace"
        partial.write_text(
            self.trace.read_text(encoding="ascii")+
            "50 E22 0036\n50 A20 1234\n",encoding="ascii")
        with self.assertRaisesRegex(ValueError,"SMALW pointer"):
            joined.analyze_join(self.clone,self.sf2,partial,0)

    def test_silent_or_unsupported_source_wav_cannot_be_certified(self):
        silent=self.root/"source-silent.wav"
        with wave.open(str(silent),"wb") as w:
            w.setnchannels(2)
            w.setsampwidth(2)
            w.setframerate(44100)
            w.writeframes(b"\x00\x00\x00\x00"*44100)
        with self.assertRaisesRegex(ValueError,"lacks musical-range"):
            joined.analyze_join(self.clone,self.sf2,self.trace,0,silent)
        wrong_rate=self.root/"source-48khz.wav"
        with wave.open(str(wrong_rate),"wb") as w:
            w.setnchannels(2)
            w.setsampwidth(2)
            w.setframerate(48000)
            w.writeframes(struct.pack("<hh",300,-300)*48000)
        with self.assertRaisesRegex(ValueError,"44.1kHz"):
            joined.analyze_join(self.clone,self.sf2,self.trace,0,wrong_rate)


if __name__=="__main__":
    unittest.main()
