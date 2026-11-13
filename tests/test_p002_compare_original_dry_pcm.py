"""Synthetic-only acceptance of P002 original Win95 versus VST3 PCM16 comparison.

Never assert that synthetic WAVs are original Creative driver recordings.
"""
from __future__ import annotations

import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import wave

from tools import p002_compare_original_dry_pcm as cmp


def samples(n=1024):
    return [(1200 + (i * 319) % 15000, -900 - (i * 127) % 11000)
            for i in range(n)]


def make_wav(path: Path, frames, *, rate=44100, channels=2, width=2):
    with wave.open(str(path), "wb") as dst:
        dst.setnchannels(channels)
        dst.setsampwidth(width)
        dst.setframerate(rate)
        if channels == 2 and width == 2:
            dst.writeframes(b"".join(struct.pack("<hh", *sample)
                                     for sample in frames))
        elif width == 1:
            dst.writeframes(bytes([128] * len(frames) * channels))
        else:
            dst.writeframes(b"\0" * len(frames) * channels * width)


class OriginalDryPcm16ComparatorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="p002-dry-")
        self.root = Path(self.temp.name)
        self.a = self.root / "original.wav"
        self.b = self.root / "native.wav"
        self.note = samples(1024)
        make_wav(self.a, [(0, 0)] * 150 + self.note + [(0, 0)] * 100)
        make_wav(self.b, self.note)

    def tearDown(self):
        self.temp.cleanup()

    def compare(self, *, start_original=150, start_vst3=0,
                frames=1024):
        return cmp.compare_pcm16_windows(
            self.a, self.b, start_original, start_vst3, frames)

    def test_identical_nonzero_note_windows_explicit_boot_silence_excluded(self):
        result = self.compare()
        self.assertTrue(result["sample_exact"])
        self.assertEqual(result["status"], "EXACT_PRECARD_PCM16_EQUAL")
        self.assertEqual(result["compared_stereo_frames"], 1024)
        self.assertEqual(result["original_start_frame"], 150)
        self.assertEqual(result["vst3_start_frame"], 0)
        self.assertEqual(result["mismatched_channel_samples"], 0)
        self.assertIsNone(result["first_mismatched_relative_frame"])
        self.assertEqual(result["rms_error_pcm16_lsb"], 0.0)
        self.assertEqual(result["original_window_sha256"],
                         result["vst3_window_sha256"])
        self.assertEqual(result["gain_normalization"], "NONE")
        self.assertEqual(result["automatic_time_alignment"], "NONE")
        self.assertEqual(result["normal_post_CT1745_float_path"],
                         "NOT_TESTED_BY_PCM16_COMPARATOR")
        self.assertFalse(result["release_approved"])

    def test_one_lsb_mismatch_and_first_frame_reported(self):
        changed = list(self.note)
        left, right = changed[37]
        changed[37] = (left + 1, right)
        make_wav(self.b, changed)
        result = self.compare()
        self.assertFalse(result["sample_exact"])
        self.assertEqual(result["status"], "PRECARD_PCM16_MISMATCH")
        self.assertEqual(result["mismatched_channel_samples"], 1)
        self.assertEqual(result["mismatched_stereo_frames"], 1)
        self.assertEqual(result["first_mismatched_relative_frame"], 37)
        self.assertEqual(result["max_abs_difference_pcm16_lsb"], 1)

    def test_gain_change_not_normalized_or_hidden(self):
        make_wav(self.b, [(left // 2, right // 2)
                          for left, right in self.note])
        result = self.compare()
        self.assertFalse(result["sample_exact"])
        self.assertGreater(result["mismatched_channel_samples"], 1000)
        self.assertGreater(result["rms_error_pcm16_lsb"], 100)
        self.assertNotEqual(result["original_window_sha256"],
                            result["vst3_window_sha256"])

    def test_one_frame_shift_is_not_auto_aligned(self):
        # The same waveform shifted by ONE sample is a mismatch until
        # an authenticated source timestamp supplies a different offset.
        make_wav(self.b, [(0, 0)] + self.note)
        result = self.compare()
        self.assertFalse(result["sample_exact"])
        self.assertEqual(result["first_mismatched_relative_frame"], 0)
        aligned = self.compare(start_vst3=1)
        self.assertTrue(aligned["sample_exact"])

    def test_channel_swap_is_never_considered_equivalent(self):
        make_wav(self.b, [(right, left) for left, right in self.note])
        result = self.compare()
        self.assertFalse(result["sample_exact"])
        self.assertEqual(result["mismatched_stereo_frames"], 1024)

    def test_silent_or_too_quiet_original_selection_not_certified(self):
        make_wav(self.a, [(0, 0)] * 2048)
        with self.assertRaisesRegex(ValueError, "silence"):
            self.compare(start_original=0)
        make_wav(self.a, [(1, 1)] * 2048)
        with self.assertRaisesRegex(ValueError, "musical-range"):
            self.compare(start_original=0)

    def test_silent_native_selection_cannot_claim_parity(self):
        make_wav(self.b, [(0, 0)] * 1024)
        with self.assertRaisesRegex(ValueError, "VST3 selection is silence"):
            self.compare()

    def test_unsupported_rate_width_channel_and_short_source_no_go(self):
        for settings in ({"rate": 48000}, {"channels": 1},
                         {"width": 1}):
            with self.subTest(settings=settings):
                make_wav(self.b, self.note, **settings)
                with self.assertRaisesRegex(ValueError, "44.1kHz"):
                    self.compare()
        make_wav(self.b, self.note[:300])
        with self.assertRaisesRegex(ValueError, "exceeds available"):
            self.compare()

    def test_unsafe_window_and_same_input_no_go(self):
        for so, sn, count in ((-1, 0, 1024), (150, -1, 1024),
                              (150, 0, 255), (150, 0, cmp.MAX_COMPARE_FRAMES + 1)):
            with self.subTest(so=so, sn=sn, count=count):
                with self.assertRaisesRegex(ValueError, "Explicit"):
                    cmp.compare_pcm16_windows(self.a, self.b, so, sn, count)
        with self.assertRaisesRegex(ValueError, "distinct"):
            cmp.compare_pcm16_windows(self.a, self.a, 150, 150, 1024)

    def test_cli_writes_private_sha_report_and_returns_nonzero_on_mismatch(self):
        report = self.root / "source-private-comparison.json"
        argv = ["p002_compare_original_dry_pcm.py",
                "--original-precard-wav", str(self.a),
                "--vst3-precard-wav", str(self.b),
                "--original-start-frame", "150",
                "--vst3-start-frame", "0",
                "--frames", "1024",
                "--private-report", str(report)]
        with patch.object(sys, "argv", argv):
            self.assertEqual(cmp.main(), 0)
        evidence = json.loads(report.read_text("utf-8"))
        self.assertTrue(evidence["sample_exact"])
        self.assertFalse(evidence["release_approved"])
        with patch.object(sys, "argv", argv):
            with self.assertRaises(SystemExit):
                cmp.main()  # refuse to overwrite private evidence

        changed = list(self.note)
        changed[-1] = (changed[-1][0], changed[-1][1] + 1)
        make_wav(self.b, changed)
        second = self.root / "source-private-mismatch.json"
        argv[-1] = str(second)
        with patch.object(sys, "argv", argv):
            self.assertEqual(cmp.main(), 1)
        self.assertEqual(
            json.loads(second.read_text("utf-8"))["mismatched_channel_samples"], 1)


if __name__ == "__main__":
    unittest.main()
