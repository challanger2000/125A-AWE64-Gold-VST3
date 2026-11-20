"""P001 fail-closed gate contract checks. No real VM run in public CI."""
import tempfile
import unittest
from pathlib import Path
import sys
import wave

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import p001_guest_capture_gate as gate


class GuestCaptureGateContract(unittest.TestCase):
    def test_win95_log_exact_five_note_pairs(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "guest.log"
            rows = [
                "P001 ORIGINAL CREATIVE WIN95 MIDI PROBE ENTRYPOINT",
                "MIDI_DEVICE 0x00000000 name=SB AWE32 MIDI Synth [620]",
                "AWE MIDI OPEN result=0x00000000",
                "ORIGINAL_CREATIVE_AWE_WINMM_SELECTED",
            ]
            for pitch, velocity in gate.NOTES:
                on = 0x90 | (pitch << 8) | (velocity << 16)
                off = 0x80 | (pitch << 8)
                rows += [f"NOTE_ON value=0x{on:08X} result=0x00000000",
                         f"NOTE_OFF value=0x{off:08X} result=0x00000000"]
            p.write_text("\r\n".join(rows) + "\r\n")
            result = gate.verify_guest_log(p)
            self.assertEqual((result["note_on"], result["note_off"]), (5, 5))
            self.assertFalse(result["helper_normal_exit_marker_observed"])
            p.write_text(p.read_text().replace("result=0x00000000",
                                                "result=0x00000001", 1))
            with self.assertRaises(ValueError):
                gate.verify_guest_log(p)

    def test_silent_boot_wav_cannot_pass_as_music(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "silent.wav"
            with wave.open(str(p), "wb") as w:
                w.setnchannels(2)
                w.setsampwidth(2)
                w.setframerate(44100)
                w.writeframes(b"\0" * (44100 * 4 * 2))
            with self.assertRaisesRegex(ValueError, "boot pop or silence"):
                gate.analyze_wav(p)

    def test_trace_beyond_capture_fails(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "bad.trace"
            p.write_text("0 E22 0000\n240000 A20 0ABC\n")
            with self.assertRaisesRegex(ValueError, "beyond WAV"):
                gate.analyze_trace(p, 200000, {1})

    def test_mismatched_rom_fingerprint_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "wrong.raw"
            p.write_bytes(b"\0" * 1048576)
            self.assertNotEqual(gate.sha256(p), gate.ROM_SHA)


if __name__ == "__main__":
    unittest.main()
