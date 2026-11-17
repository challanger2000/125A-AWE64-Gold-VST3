"""Synthetic QA for P003 evidence audit (no private Creative data in CI)."""
import sys
import tempfile
from pathlib import Path
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from p003_audit_original_stereo_mode_gate_private import (
    research_prediction, audit_owner, checksum)

class OriginalModeGateResearchTests(unittest.TestCase):
    def test_original_above_two_skips_extra_gate(self):
        self.assertEqual(research_prediction(93,4,0),93)
        self.assertEqual(research_prediction(91,4,1),91)
        self.assertEqual(research_prediction(95,3,1),95)

    def test_original_at_two_executes_extra_gate(self):
        self.assertEqual(research_prediction(93,2,0),107)
        self.assertEqual(research_prediction(91,2,1),141)
        self.assertEqual(research_prediction(95,2,1),145)

    def test_cannot_approve_unrecognized_original_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'untrusted.zip'
            p.write_bytes(b'not the pinned original')
            self.assertEqual(len(checksum(p)),64)
            with self.assertRaisesRegex(ValueError,'checksum'):
                audit_owner(p,'0'*64)

    def test_research_formula_is_not_production_timing(self):
        # The seven documented unresolved +3-cycle cases must remain
        # unresolved rather than being silently absorbed in a baseline.
        predicted=research_prediction(93,2,0)
        self.assertEqual(110-predicted,3)

if __name__=='__main__':
    unittest.main()
