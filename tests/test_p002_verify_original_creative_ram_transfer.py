"""Synthetic only: P002 observational ORIGINAL Creative RAM-bank transform model.

No original Win95, owner SF2, 86Box private trace or ROM is in CI.
The model does NOT qualify RAM SoundFont VST3 production playback.
"""
from __future__ import annotations

import struct
import hashlib
from unittest.mock import patch
import tempfile
import unittest
from pathlib import Path

from tools import p002_verify_original_creative_ram_transfer as original


class OriginalCreativeSourceRamTransferTests(unittest.TestCase):
    def setUp(self):
        self.source=(
            0,300,0,-400,100,0,0,250,0,0,0,-500,0,0,
            1000,-600,0,500,0,0,0,200,-200,0,0,50,0,0,0,0
        )
        self.predicted=original.predicted_creative_source_words(self.source)
        self.base=0x200010
        self.trace=[
            "0 E22 0036","0 E20 0000","0 A20 0000","0 A22 8000",
            "0 E22 003A", "0 A20 AA55",
            "1 E22 0036", "1 620 1234",
            "1 A20 0010","1 A22 0020","2 E22 003A",
        ]+[f"{2+i} A20 {v:04X}" for i,v in enumerate(self.predicted)]

    def replay(self, trace=None):
        return original.observed_final_ram_words(
            trace if trace is not None else self.trace,
            self.base,len(self.source))

    def test_source_zero_padding_len_three_and_single_zero(self):
        pred=self.predicted
        self.assertEqual(len(pred),len(self.source))
        self.assertEqual(pred[0],0)
        self.assertEqual(pred[-1],0)
        # Isolated zero is NOT forcibly zero; the original three-tap
        # formula is applied when zero-run length is one or two.
        self.assertEqual(pred[2],((0-300+400)>>3)&65535)
        self.assertEqual(pred[5],((-100)>>3)&65535)
        self.assertEqual(pred[8],0)  # three consecutive zero words
        self.assertEqual(pred[9],0)
        self.assertEqual(pred[10],0)

    def test_literal_source_write_replay_ignores_out_of_ram_probes(self):
        proof=self.replay()
        self.assertEqual(proof["source_ram_words"],list(self.predicted))
        self.assertEqual(proof["original_out_of_range_probe_writes"],1)
        self.assertEqual(proof["observed_source_window_writes"],
                         len(self.source))
        self.assertEqual(proof["source_window_overwritten_words"],0)
        self.assertEqual(proof["actual_original_register_events"],
                         len(self.trace))

    def test_real_source_final_rewrites_observed_not_hidden(self):
        rewritten=self.trace+[
            "99 E22 0036","99 A20 0017","99 A22 0020",
            "100 E22 003A",
            f"100 A20 {self.predicted[7]:04X}",
        ]
        proof=self.replay(rewritten)
        self.assertEqual(proof["source_ram_words"],list(self.predicted))
        self.assertEqual(proof["source_window_overwritten_words"],1)
        self.assertEqual(proof["observed_source_window_writes"],
                         len(self.source)+1)

    def test_one_lsb_error_is_no_longer_bit_exact(self):
        forged=list(self.trace)
        forged[-2]=forged[-2][:-4]+f"{(self.predicted[-2]^1):04X}"
        proof=self.replay(forged)
        self.assertNotEqual(proof["source_ram_words"],list(self.predicted))

    def test_fail_closed_on_missing_pointer_illegal_ports_and_wrong_probe(self):
        for tail,pattern in (
            (["100 E22 0036","100 A20 1000","100 E22 003A"],
             "SMALW"),
            (["100 E22 0036","100 A24 1234"],"SMALW"),
            (["100 E22 003A","100 A22 1234"],"SMLD"),
            (["100 E22 0036","100 A20 0000","100 A22 8000",
              "100 E22 003A","100 A20 DEAD"],"off-RAM"),
        ):
            with self.subTest(tail=tail):
                with self.assertRaisesRegex(ValueError,pattern):
                    self.replay(self.trace+tail)
        # Source word is never guessed from an absent final physical write.
        with self.assertRaisesRegex(ValueError,"lacks one or more"):
            self.replay(self.trace[:-1])

    def test_exact_private_original_guest_log_bytes_equal_file_without_copy(self):
        # Owner's FAT16 extractor returns the actual guest P002RAM.LOG as
        # in-memory bytes. The transfer proof must use those SAME bytes
        # and SHA as its file-based counterpart, with no private temp file.
        guest=b"ORIGINAL WIN95 SYNTHETIC GUEST LOG\\r\\n"
        with tempfile.TemporaryDirectory(prefix="p002-original-guest-") as td:
            root=Path(td)
            fake_trace=root/"synthetic.trace"
            fake_bank=root/"synthetic.sf2"
            guest_file=root/"synthetic.log"
            fake_trace.write_bytes(b"TRACE")
            fake_bank.write_bytes(b"SF2")
            guest_file.write_bytes(guest)
            expected_word_count=len(self.source)+16
            original_samples=original.predicted_creative_source_words(
                self.source)
            def validate_guest(data,bank):
                self.assertEqual(data,guest)
                self.assertEqual(bank,b"SF2")
                return {"original_driver_ram_allocated_words":
                        expected_word_count}
            def fake_replay(lines,base,count):
                self.assertEqual(base,self.base)
                self.assertEqual(count,len(self.source))
                return {
                    "source_ram_words":list(original_samples),
                    "source_window_overwritten_words":255,
                    "observed_source_window_writes":len(self.source)+255,
                }
            with (patch.object(original,"audit",side_effect=validate_guest),
                  patch.object(original,"source_smpl",return_value=self.source),
                  patch.object(original,"observed_final_ram_words",
                               side_effect=fake_replay)):
                from_file=original.analyze(
                    fake_trace,fake_bank,guest_file,self.base)
                from_bytes=original.analyze(
                    fake_trace,fake_bank,guest,self.base)
                self.assertEqual(from_file,from_bytes)
                self.assertEqual(
                    from_bytes["original_guest_log_sha256"],
                    hashlib.sha256(guest).hexdigest())
                self.assertEqual(
                    from_bytes["source_window_overwritten_words"],255)
                with self.assertRaisesRegex(ValueError,
                                            "guest log missing or oversized"):
                    original.analyze(
                        fake_trace,fake_bank,b"A"*131073,self.base)
                with self.assertRaisesRegex(ValueError,
                                            "guest log missing or oversized"):
                    original.analyze(fake_trace,fake_bank,b"",self.base)

    def test_source_guard_checks_and_zero_rule_not_universal(self):
        for source in (
            (123,300,0), (0,100,999), (0,)
        ):
            with self.subTest(source=source):
                with self.assertRaises(ValueError):
                    original.predicted_creative_source_words(source)
        with self.assertRaisesRegex(ValueError,"source sample RAM address"):
            original.observed_final_ram_words(self.trace,0x100000,10)


if __name__=="__main__":
    unittest.main()
