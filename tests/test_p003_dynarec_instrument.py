"""P003 Dynarec CPU block probe: fail-closed, pinned source edits only."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from instrument_p003_86box_dynarec_clock import (
    ANCHOR_MACRO, ANCHOR_PRE_EXEC, ANCHOR_POST_EXEC, ANCHOR_POST_TSC,
    ANCHOR_CACHED, ANCHOR_RECOMPILE, ANCHOR_NEW, instrument, git_blob_sha)

SOURCE=("\n".join([
    "#include <stdio.h>",
    "#include <stdint.h>",
    "#include <stdlib.h>",
    ANCHOR_MACRO,
    "void synthetic(void){",
    ANCHOR_CACHED,
    ANCHOR_RECOMPILE,
    ANCHOR_NEW,
    ANCHOR_PRE_EXEC,
    ANCHOR_POST_EXEC,
    ANCHOR_POST_TSC,
    "}",
])+"\n").encode()

class DynarecInstrumentationTest(unittest.TestCase):
    def test_existing_source_only_instrumentation(self):
        out=instrument(SOURCE,expected_sha=None)
        self.assertEqual(out.count(b"P003_DYNBLK_BLOCK_CLOCK_V1"),2)
        self.assertIn(b"AWE64_P003_DYNAREC_BLOCK_TRACE",out)
        self.assertIn(b"AWE64_P003_DYNAREC_LCOD_BASE",out)
        self.assertIn(b"p003_dynblk_clock('X'",out)
        self.assertIn(b"p003_dynblk_clock('S'",out)
        for kind in (0,1,2,3):
            self.assertIn(f"p003_dynblk_path = {kind}".encode(),out)
        self.assertEqual(out.count(ANCHOR_CACHED.encode()),1)
        self.assertEqual(out.count(ANCHOR_NEW.encode()),1)

    def test_source_hash_pin(self):
        with self.assertRaisesRegex(ValueError,"SHA1 mismatch"):
            instrument(SOURCE)
        self.assertEqual(instrument(SOURCE,expected_sha=git_blob_sha(SOURCE)),
                         instrument(SOURCE,expected_sha=None))

    def test_reject_double_instrumentation(self):
        once=instrument(SOURCE,expected_sha=None)
        with self.assertRaisesRegex(ValueError,"repeated"):
            instrument(once,expected_sha=None)

    def test_reject_modified_original_cpu(self):
        for anchor in (ANCHOR_PRE_EXEC,ANCHOR_POST_TSC,
                       ANCHOR_CACHED,ANCHOR_RECOMPILE,ANCHOR_NEW):
            with self.subTest(anchor=anchor[:25]):
                altered=SOURCE.replace(anchor.encode(),b"/* anchor lost */",1)
                with self.assertRaisesRegex(ValueError,"anchor"):
                    instrument(altered,expected_sha=None)

    def test_reject_duplicate_callsite(self):
        altered=SOURCE+ANCHOR_CACHED.encode()
        with self.assertRaisesRegex(ValueError,"duplicated"):
            instrument(altered,expected_sha=None)

if __name__=="__main__":
    unittest.main()
