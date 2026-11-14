"""Synthetic-only native/Win95 register OUT-window differential regressions.

These fixtures are NOT original Creative source data or audio parity.
"""
import json
import tempfile
from pathlib import Path
import unittest
from tools import p002_compare_original_native_ram_note_ports as cmp

class NativeRamOriginalPortWindowContract(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix="p002-port-original-")
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.original=self.root/"original.trace"
        self.native=self.root/"native.trace"
        self.original.write_bytes((
            "99 E22 0\n"
            "100 E22 0\n100 A20 23F8\n"
            "R 100 E20 1234\n"
            "101 E22 C0\n101 620 F8D7\n101 622 23\n"
            "101 E22 E0\n101 620 FA68\n").encode("ascii"))
        self.lines=["3 E22 0","3 A20 23F8","4 E22 C0",
                    "4 620 F8D7","4 622 23","4 E22 E0","4 620 FA68"]
        self.write_native(self.lines)

    def write_native(self,lines,header=True):
        p=((cmp.HEADER+cmp.CLOCK_HEADER).decode("ascii") if header else "")
        # The REAL VST3 writes exact LF using Win32 WriteFile. Python
        # write_text() re-translates LF to CRLF on Windows, so tests
        # must preserve those actual bytes rather than fabricating a
        # stale header rejected by the strict evidence parser.
        self.native.write_bytes((p+"\n".join(lines)+"\n").encode("ascii"))

    def compare(self,**kwargs):
        return cmp.compare_windows(
            self.original,self.native,
            original_origin=kwargs.get("source",100),
            native_origin=kwargs.get("native",3),
            frames=kwargs.get("frames",2),
            require_owner_original=kwargs.get("require_sha",False))

    def test_exact_write_match_with_read_non_claim(self):
        r=self.compare()
        self.assertEqual(r["status"],"EQUAL_ORIGINAL_NATIVE_RAM_NOTE_OUT_WINDOW")
        self.assertEqual(r["original_write_count"],7)
        self.assertEqual(r["native_write_count"],7)
        self.assertTrue(r["sample_exact_16bit_port_writes"])
        self.assertFalse(r["reads_compared"])
        self.assertFalse(r["original_vs_native_dry_pcm_equal"])
        self.assertFalse(r["release_approved"])

    def test_one_bit_flip_reports_exact_first_difference(self):
        v=list(self.lines);v[3]="4 620 F8D6";self.write_native(v)
        r=self.compare()
        self.assertEqual(r["first_mismatch"]["index"],3)
        self.assertEqual(r["first_mismatch"]["field"],"value")

    def test_one_chip_frame_or_out_order_shift_rejected(self):
        self.assertEqual(self.compare(native=2)["first_mismatch"]["field"],
                         "relative_frame")
        v=list(self.lines);v[2],v[3]=v[3],v[2]
        self.write_native(v)
        self.assertEqual(self.compare()["first_mismatch"]["field"],"port")

    def test_missing_and_extra_out_rejected(self):
        self.write_native(self.lines[:-1])
        self.assertEqual(self.compare()["first_mismatch"]["field"],
                         "missing_or_extra_write")
        self.write_native(self.lines+["4 E22 10"])
        self.assertEqual(self.compare()["first_mismatch"]["field"],
                         "missing_or_extra_write")

    def test_matching_selector_only_is_not_a_musical_register_proof(self):
        # Previously these identical, non-empty traces returned EQUAL
        # even though neither side programmed a single EMU8000 value.
        self.original.write_bytes(b"100 E22 0\n")
        self.write_native(["3 E22 0"])
        with self.assertRaisesRegex(ValueError,
                                    "Original note window lacks chip selector"):
            self.compare()
        self.original.write_bytes(b"100 E22 0\n100 A20 23F8\n")
        with self.assertRaisesRegex(ValueError,
                                    "native note window lacks chip selector"):
            self.compare()
        # Conversely, a raw data-port write without the chip selector
        # cannot stand alone as a source-backed note's register event.
        self.write_native(["3 A20 23F8"])
        with self.assertRaisesRegex(ValueError,
                                    "non-RAM EMU8000 voice data write"):
            self.compare()

    def test_identical_ram_upload_must_not_be_misreported_as_note_parity(self):
        # Identical SMALW/SMLD upload writes prove neither note allocation
        # nor any voice-register state, even at exactly matching clocks.
        ram=["3 E22 36","3 A20 0010","3 A22 0020",
             "4 E22 3A","4 A20 1111"]
        self.original.write_bytes(("\n".join(
            line.replace("3 ","100 ").replace("4 ","101 ")
            for line in ram)+"\n").encode("ascii"))
        self.write_native(ram)
        with self.assertRaisesRegex(ValueError,
                                    "non-RAM EMU8000 voice data write"):
            self.compare()

    def test_unselected_data_before_selector_is_not_a_note(self):
        # A stale pre-window register latch is not source provenance.
        self.original.write_bytes(b"100 A20 1234\n100 E22 0\n")
        self.write_native(["3 A20 1234","3 E22 0"])
        with self.assertRaisesRegex(ValueError,
                                    "non-RAM EMU8000 voice data write"):
            self.compare()

    def test_wrong_original_sha_and_missing_exporter_header_no_go(self):
        with self.assertRaisesRegex(ValueError,"owner verified original"):
            self.compare(require_sha=True)
        self.write_native(self.lines,header=False)
        with self.assertRaisesRegex(ValueError,"recorder header"):
            self.compare()

    def test_no_auto_align_empty_and_duplicate_input_rejected(self):
        with self.assertRaisesRegex(ValueError,"no source/native writes"):
            self.compare(source=400,native=500)
        with self.assertRaisesRegex(ValueError,"distinct"):
            cmp.compare_windows(self.original,self.original,
                                original_origin=100,native_origin=100,frames=2)
        with self.assertRaisesRegex(ValueError,"frame origins"):
            self.compare(frames=0)

    def test_cli_writes_new_private_report_once(self):
        out=self.root/"native-port-report.json"
        cli=["--original",str(self.original),"--native",str(self.native),
             "--original-origin","100","--native-origin","3",
             "--frames","2","--private-report",str(out),
             "--allow-synthetic-source-only-for-tests"]
        self.assertEqual(cmp.main(cli),0)
        result=json.loads(out.read_text(encoding="utf-8"))
        self.assertFalse(result["release_approved"])
        self.assertTrue(result["sample_exact_16bit_port_writes"])
        with self.assertRaises(SystemExit):
            cmp.main(cli)

if __name__=="__main__":
    unittest.main()
