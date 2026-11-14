"""Synthetic-only checks of empirically observed original Creative RAM filter.

No original user-owned SoundFont, Creative driver, ROM, WAV or Win95 bytes.
A positive match validates one captured trace shape, NOT a general SF2 DSP.
"""
from __future__ import annotations
import hashlib
from pathlib import Path
import struct
import tempfile
import unittest

from tools import p002_original_bank_transform_evidence as research


def riff_chunk(tag: bytes, payload: bytes):
    return tag + struct.pack("<I", len(payload)) + payload + (b"\0" if len(payload) & 1 else b"")


def sf2_test_bank(words, *, loop_end_plus_two=False, bad_sample_type=False,
                  sm24=False):
    pcm = struct.pack(f"<{len(words)}h", *words)
    sample_end = len(words)-3
    loop_end = sample_end+2 if loop_end_plus_two else sample_end-1
    shdr = struct.pack(
        "<20sIIIII BbHH",
        b"synthetic_source", 0, sample_end, 1,
        loop_end, 44100, 60, 0, 0, 2 if bad_sample_type else 1)
    eos = struct.pack("<20sIIIII BbHH", b"EOS", len(words), len(words),
                      len(words), len(words), 44100, 60, 0, 0, 1)
    assert len(shdr) == len(eos) == 46
    info = riff_chunk(b"LIST", b"INFO" + riff_chunk(b"ifil", b"\x02\0\0\0"))
    sdta = riff_chunk(
        b"LIST", b"sdta" + riff_chunk(b"smpl", pcm) +
        (riff_chunk(b"sm24", b"\0"*len(words)) if sm24 else b""))
    pdta = riff_chunk(b"LIST", b"pdta" + riff_chunk(b"shdr", shdr+eos))
    return riff_chunk(b"RIFF", b"sfbk"+info+sdta+pdta)


# Synthetic signed PCM. Isolated zeros, two-zero runs, three-zero runs
# and signed-negative arithmetic shifts are all independently checked.
SOURCE = (0, 400, 0, 80, 0, 0, -160, 0, 0, 0,
          320, -160, 1000, -250, 0, -100, 60, 0,
          0, 80, 0, -240, 0, 0)


def independent_expected(words):
    # A deliberately SIMPLE scalar oracle, separate from the production
    # research implementation. Long-zero runs are zeroed by explicit
    # inspection of each maximal silence segment.
    output = []
    for idx, x in enumerate(words):
        prev = words[idx-1] if idx else 0
        nxt = words[idx+1] if idx+1 < len(words) else 0
        result = (6*x-prev-nxt)//8
        if idx == 0:
            result = 0
        else:
            a = idx
            while a > 0 and words[a-1] == 0:
                a -= 1
            b = idx
            while b+1 < len(words) and words[b+1] == 0:
                b += 1
            if x == 0 and b-a+1 >= 3:
                result = 0
        output.append(result)
    return output


def authentic_port_shaped_trace(words, *, guard_words=16, changed=None,
                               add_rewrite=False):
    # Original register-selection and memory-data port values, no driver.
    values = [0]*guard_words + list(words) + [0]*15
    out = ["0 E22 0036", "0 A20 0000", "0 A22 0020", "0 E22 003A"]
    for idx, x in enumerate(values):
        if changed is not None and idx == guard_words+changed:
            x = x ^ 1
        out.append(f"1 A20 {x & 0xFFFF:04X}")
    if add_rewrite:
        index = guard_words+3
        out.extend(["2 E22 0036", f"2 A20 {index:04X}",
                    "2 A22 0020", "3 E22 003A",
                    "3 A20 0000",  # source guest transient real rewrite
                    "4 E22 0036", f"4 A20 {index:04X}",
                    "4 A22 0020", "5 E22 003A",
                    f"5 A20 {values[index] & 0xFFFF:04X}"])
    return "\n".join(out)+"\n"


class OriginalCreativeBankTransformTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="p002-synthetic-bank-")
        self.root = Path(self.temp.name)
        self.sf2 = self.root/"synthetic-bank.sf2"
        self.trace = self.root/"synthetic-source-emu8k.trace"
        self.sf2.write_bytes(sf2_test_bank(SOURCE, loop_end_plus_two=True))
        self.trace.write_text(authentic_port_shaped_trace(
            independent_expected(SOURCE), add_rewrite=True), encoding="ascii")

    def tearDown(self):
        self.temp.cleanup()

    def test_exact_source_transform_and_real_ram_rewrite_shape(self):
        pcm, count = research.original_bank_pcm16(self.sf2.read_bytes())
        self.assertEqual(count, 1)
        self.assertEqual(pcm, SOURCE)
        pred, details = research.observed_three_tap_silent_guard_hypothesis(SOURCE)
        decoded = list(struct.unpack(f"<{len(SOURCE)}h", pred))
        self.assertEqual(decoded, independent_expected(SOURCE))
        self.assertEqual(decoded[2], -60)  # isolated 0 filtered
        self.assertEqual(decoded[4], -10)  # first zero of exactly 2
        self.assertEqual(decoded[5], 20)   # second zero of exactly 2
        self.assertEqual(decoded[7:10], [0,0,0])  # long silence preserved
        self.assertGreater(details["source_silent_runs_at_least_three"], 0)
        report = research.verify(self.trace, self.sf2, 16)
        self.assertEqual(report["status"],
                         "EXACT_OBSERVED_SINGLE_BANK_TRANSFORM_MATCH")
        self.assertEqual(report["mismatched_final_ram_words"], 0)
        self.assertEqual(report["sample_window_written_words"], len(SOURCE))
        self.assertEqual(report["sample_window_overwritten_physical_words"], 1)
        self.assertEqual(report["physical_word_rewrite_events"], 2)
        self.assertEqual(report["changed_value_rewrite_events"], 2)
        self.assertEqual(report["hypothesized_pcm16_sha256"],
                         report["observed_final_ram_window_sha256"])
        self.assertFalse(report["proven_for_other_sf2_banks"])
        self.assertFalse(report["native_vst3_ram_sf2_sound_ready"])
        self.assertFalse(report["release_approved"])

    def test_one_bit_mutation_is_not_hidden_by_apparent_pcm_similarity(self):
        self.trace.write_text(authentic_port_shaped_trace(
            independent_expected(SOURCE), changed=12), encoding="ascii")
        report = research.verify(self.trace, self.sf2, 16)
        self.assertEqual(report["mismatched_final_ram_words"], 1)
        self.assertEqual(report["status"], "ORIGINAL_BANK_TRANSFORM_MISMATCH")
        self.assertNotEqual(report["hypothesized_pcm16_sha256"],
                            report["observed_final_ram_window_sha256"])

    def test_wrong_guard_offset_refused_and_no_automatic_realign(self):
        for wrong in (0, 15, 17, 128):
            with self.subTest(guard=wrong):
                bad = research.verify(self.trace, self.sf2, wrong)
                self.assertGreater(bad["mismatched_final_ram_words"], 0)
        with self.assertRaisesRegex(ValueError, "guard offset"):
            research.verify(self.trace, self.sf2, -1)

    def test_mono_soundfont_loop_guard_two_words_accepted_beyond_end(self):
        src, count = research.original_bank_pcm16(self.sf2.read_bytes())
        self.assertEqual(src, SOURCE)
        self.assertEqual(count, 1)
        malformed = bytearray(self.sf2.read_bytes())
        idx = malformed.index(b"shdr")+8
        # Original-source loop end +3, beyond precisely bounded guard.
        struct.pack_into("<I", malformed, idx+20+12, len(SOURCE))
        self.sf2.write_bytes(malformed)
        with self.assertRaisesRegex(ValueError, "sample header"):
            research.original_bank_pcm16(self.sf2.read_bytes())

    def test_invalid_sample_type_and_unsupported_sm24_no_go(self):
        for kw in ({"bad_sample_type":True}, {"sm24":True}):
            with self.subTest(setting=kw):
                self.sf2.write_bytes(sf2_test_bank(SOURCE,**kw))
                with self.assertRaisesRegex(ValueError,
                                            "mono sample header|24-bit"):
                    research.original_bank_pcm16(self.sf2.read_bytes())

    def test_invalid_hardware_port_and_trace_truncation_no_go(self):
        invalid = self.trace.read_text().replace(
            "0 A22 0020", "0 A24 0020", 1)
        self.trace.write_text(invalid)
        with self.assertRaises(ValueError):
            research.verify(self.trace, self.sf2, 16)
        self.trace.write_text("0 E22 0036\n0 A20 1000\n")
        with self.assertRaises(ValueError):
            research.verify(self.trace, self.sf2, 16)

    def test_missing_last_physical_sample_word_never_accepted(self):
        # A source bank may legitimately end in ZERO. A naive byte
        # comparison would accept unwritten zero RAM as identical.
        # Explicitly remove the last source-word write and all trailing
        # guard writes rather than substituting a READ (which does not
        # advance the original EMU8000 RAM pointer).
        initial = authentic_port_shaped_trace(
            independent_expected(SOURCE), add_rewrite=False).splitlines()
        complete_until_penultimate = initial[:4+16+len(SOURCE)-1]
        self.trace.write_text(
            "\n".join(complete_until_penultimate)+"\n", encoding="ascii")
        with self.assertRaisesRegex(
                ValueError, "Unwritten physical sample words"):
            research.verify(self.trace, self.sf2, 16)

    def test_invalid_source_first_guard_and_sf2_length_refused(self):
        bad = list(SOURCE)
        bad[0] = 42
        with self.assertRaisesRegex(ValueError, "first-word"):
            research.observed_three_tap_silent_guard_hypothesis(bad)
        sf2 = bytearray(self.sf2.read_bytes())
        sf2[4] ^= 1
        self.sf2.write_bytes(sf2)
        with self.assertRaisesRegex(ValueError, "RIFF identity"):
            research.original_bank_pcm16(self.sf2.read_bytes())


if __name__ == "__main__":
    unittest.main()
