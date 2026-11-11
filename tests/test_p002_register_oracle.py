import importlib.util
from pathlib import Path
import tempfile
import unittest

SCRIPT=Path(__file__).resolve().parent.parent/"tools/p002_register_oracle.py"
spec=importlib.util.spec_from_file_location("p002_oracle",SCRIPT)
oracle=importlib.util.module_from_spec(spec)
spec.loader.exec_module(oracle)

A=[(11,"WRITE",0xe22,0x0f1),(11,"WRITE",0xa20,0x7f7f),
   (12,"READ",0x620,0x80),(13,"WRITE",0xe22,0x031)]

class RegisterOracleContract(unittest.TestCase):
    def test_ordered_reference_literal_pass(self):
        self.assertEqual(oracle.compare(A,A[:])["status"],"REGISTER_SEQUENCE_EQUAL")
    def test_first_timing_mismatch(self):
        x=A[:];x[1]=(12,*x[1][1:])
        self.assertEqual(oracle.compare(A,x)["field"],"frame")
        self.assertEqual(oracle.compare(A,x)["index"],1)
    def test_first_direction_mismatch(self):
        x=A[:];x[2]=(12,"WRITE",0x620,0x80)
        self.assertEqual(oracle.compare(A,x)["field"],"direction")
    def test_wrong_port(self):
        x=A[:];x[0]=(11,"WRITE",0xe20,0x0f1)
        self.assertEqual(oracle.compare(A,x)["field"],"port")
    def test_one_bit_mismatch(self):
        x=A[:];x[1]=(11,"WRITE",0xa20,0x7f7e)
        self.assertEqual(oracle.compare(A,x)["field"],"value")
    def test_reordered_same_clock(self):
        x=A[:];x[0],x[1]=x[1],x[0]
        self.assertEqual(oracle.compare(A,x)["index"],0)
    def test_missing_event(self):
        self.assertEqual(oracle.compare(A,A[:-1])["field"],"missing_event")
    def test_extra_event(self):
        self.assertEqual(oracle.compare(A,A+[(13,"WRITE",0xa20,1)])["field"],"missing_event")
    def test_empty_rejected(self):
        with self.assertRaises(ValueError):oracle.compare([],[])
    def test_trace_parser_rejects_nonmonotonic(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/"bad.trace"
            p.write_text("11 E22 00FF\n10 A20 00FF\n")
            with self.assertRaisesRegex(ValueError,"non-monotonic"):
                oracle.read_trace(p)
    def test_trace_parser_preserves_read_write_order(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/"valid.trace"
            p.write_text("# original\n11 E22 F1\nR 11 620 0080\n")
            self.assertEqual(oracle.read_trace(p),
                [(11,"WRITE",0xe22,0xf1),(11,"READ",0x620,0x80)])
    def test_parser_rejects_hex_frame(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/"bad.trace"
            p.write_text("0x11 E22 F1\n")
            with self.assertRaisesRegex(ValueError,"syntax"):
                oracle.read_trace(p)

if __name__=="__main__":unittest.main()
