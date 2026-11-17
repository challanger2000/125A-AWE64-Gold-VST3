"""Synthetic adversarial tests for public source-only P003 private original auditor.

No private Windows-95/Creative bytes used. Exact original CHRONOLOGY
in fabricated 17-field lines is tested; only real owner ZIPs may
authenticate the historical 11756-event result.
"""
import sys
from pathlib import Path
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p003_audit_original_mode0_complete_release_private import (
    LCOD,verify_stream,step_spec)

def event(seq,pc,site,port,value,kind="W",tsc=1000):
    # Guest port probe original source schema uses exactly 17 tokens.
    parts=[
        kind,str(tsc),"0","0028",f"{pc:08X}",f"{port:04X}",
        f"{value:04X}",str(seq),"C13EF868",f"{LCOD+site:08X}",
        "C1179C1F","C115F321","FFFFFFFF","00000000","00000001",
        "C0010908","00000E22"
    ]
    assert len(parts)==17
    return (" ".join(parts)+"\n").encode("ascii")

def fixture(modification=None):
    """Exactly 32 synthetic releases with their genuine source-site chains."""
    rows=[]
    n=100
    for voice in range(32):
        rows.append(event(n,0xC1018F3F,0x3F13,0x0E22,
                          0x60|voice,tsc=1000+voice*200))
        n+=1
        rows.append(event(n,0xC1018EAB,0x3FCE,0x0E22,
                          0xA0|voice,tsc=1010+voice*200))
        start=n
        for idx in range(1,5):
            site,helper,port,value=step_spec(voice,idx)
            rows.append(event(n+idx,helper,site,port,value,
                              tsc=1010+voice*200+(idx>=2)*94))
        n+=5
    if modification:
        modification(rows)
    return rows

class OriginalMode0ReleaseAuditTests(unittest.TestCase):
    def test_source_and_all_32_voices(self):
        result=verify_stream(fixture())
        self.assertEqual(result["selections"],32)
        self.assertEqual(result["correlated_followup_writes"],128)
        self.assertEqual(result["physical_30_selections"],1)
        self.assertEqual(result["physical_31_selections"],1)
        # Synthetic voice29 release follows its own VTFT scan, once.
        self.assertEqual(result["ranked_after_last_voice29"],1)
        self.assertEqual(result["distinct_tsc_gaps"],1)

    def test_neighbor_voice_swap_mutant_is_rejected(self):
        def corrupt(lines):
            # Voice 31 VTFT selector erroneously targets voice 30.
            k=31*6+3
            lines[k]=event(100+31*6+3,0xC1018F12,0x3E0C,0xE22,0x7E)
        with self.assertRaisesRegex(ValueError,"mismatch"):
            verify_stream(fixture(corrupt))

    def test_wrong_a20_word_mutant_is_rejected(self):
        def corrupt(lines):
            lines[2]=event(102,0xC1018EC4,0x3FCE,0xA20,0x00FE)
        with self.assertRaisesRegex(ValueError,"mismatch"):
            verify_stream(fixture(corrupt))

    def test_missing_last_write_is_rejected(self):
        def corrupt(lines):lines.pop()
        with self.assertRaisesRegex(ValueError,"missing subsequent"):
            verify_stream(fixture(corrupt))

    def test_wrong_source_pc_mutant_is_rejected(self):
        def corrupt(lines):
            lines[1]=event(101,0xC1018EAB,0x3F52,0x0E22,0xA0)
        with self.assertRaisesRegex(ValueError,"coverage|selector"):
            verify_stream(fixture(corrupt))

    def test_nonmonotone_tsc_is_rejected(self):
        def corrupt(lines):
            lines[3]=event(103,0xC1018F12,0x3E0C,0xE22,0x60,tsc=1)
        with self.assertRaisesRegex(ValueError,"TSC regressed"):
            verify_stream(fixture(corrupt))

    def test_read_substituted_for_release_write_is_rejected(self):
        def corrupt(lines):
            lines[2]=event(102,0xC1018EC4,0x3FCE,0xA20,0xFF,kind="R")
        with self.assertRaisesRegex(ValueError,"mismatch"):
            verify_stream(fixture(corrupt))

if __name__=="__main__":
    unittest.main()
