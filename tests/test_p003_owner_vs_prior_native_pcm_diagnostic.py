"""Public-data adversarial tests for P003 owner-only PCM diagnostic."""
import importlib.util
from pathlib import Path
import struct
import unittest

CODE=Path(__file__).resolve().parents[1]/"tools"/"p003_owner_vs_prior_native_pcm_diagnostic.py"
spec=importlib.util.spec_from_file_location("p003_diag",CODE)
d=importlib.util.module_from_spec(spec)
spec.loader.exec_module(d)


class SourceOnlyTests(unittest.TestCase):
    def test_pcm_equal_mutated_one_lsb_and_silence(self):
        source=struct.pack("<h",350)*512
        self.assertTrue(d.compare_pcm(source,source)["bit_exact"])
        corrupt=bytearray(source)
        corrupt[16]^=1
        result=d.compare_pcm(source,corrupt)
        self.assertEqual(result["different_channel_samples"],1)
        self.assertEqual(result["first_difference"],
                         {"relative_frame":4,"channel":0})
        self.assertFalse(result["bit_exact"])
        with self.assertRaises(ValueError):
            d.compare_pcm(bytes(1024),bytes(1024))

    def test_port_one_sample_jitter_rejected(self):
        original=[(5,"E22",0x60),(6,"A20",0xff)]
        correct=[(20,"E22",0x60),(21,"A20",0xff)]
        jitter=[(20,"E22",0x60),(20,"A20",0xff)]
        self.assertTrue(d.register_groups(original,correct,5,20,2,2)
                        ["bit_and_clock_exact"])
        self.assertFalse(d.register_groups(original,jitter,5,20,2,2)
                         ["bit_and_clock_exact"])
        with self.assertRaises(ValueError):
            d.register_groups(original,correct[:1],5,20,2,2)

    def test_source_read_vs_write_and_order(self):
        lines=b"# chip clock 44100\n5 E22 0060\nR 5 A20 0001\n6 620 FFFF\n"
        self.assertEqual(d.writes(lines),
                         [(5,"E22",0x60),(6,"620",0xffff)])
        with self.assertRaises(ValueError):
            d.writes(b"6 620 FFFF\n5 E22 0060\n")


if __name__=="__main__":
    unittest.main()
