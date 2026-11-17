"""P108 real-86Box code patcher negative tests; entirely synthetic source fixtures."""
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p108_direct_executed_jcc_flags import patch,ADDED,FUNCTION_ANCHOR,TAIL_ANCHOR,P108_MARKER

SOURCE=(
    '#    include "x86_flags.h"\n'
    '/* P107_OLD_DYNAREC_LCOD_EXECUTED_V1 */\n'
    '/* P107_OLD_DYNAREC_INTERPRETER_FALLBACK_V2 */\n'
    +FUNCTION_ANCHOR+'\n'
    '    if (!p107_x86_trace_file)return;\n'
    +TAIL_ANCHOR
).encode("utf8")

class TestDirectOriginalJcc(unittest.TestCase):
    def test_original_five_lazy_flag_equations_and_attached_guest_callback(self):
        raw=patch(SOURCE)
        self.assertIn(P108_MARKER.encode(),raw)
        self.assertEqual(raw.count(b"p108_record_executed_branch_predicate(guest_pc);"),1)
        for x in [
           b'off == 0x4027u) taken = !zf',
           b'off == 0x40cbu) taken = !cf && !zf',
           b'off == 0x40e4u) taken = zf',
           b'off == 0x40f1u) taken = !cf',
           b'taken = cf;         /* JB',
           b'ZF_SET() ? 1 : 0',
           b'CF_SET() ? 1 : 0',
           b'"AWE64_P108_DIRECT_BRANCH_TRACE"',
           b'(p107_x86_trace_seq - 1ULL)',
        ]:
            self.assertIn(x,raw)
        self.assertEqual(raw.count(b'p108_record_executed_branch_predicate(guest_pc);'),1)
        self.assertNotIn(b'writemem',raw.lower())
        self.assertNotIn(b'emu8k_',raw.lower())
    def test_default_off_and_source_idempotence_rejected(self):
        raw=patch(SOURCE)
        self.assertIn(b'if (!path || !*path || !p107_x86_probe_enabled) return;',raw)
        with self.assertRaisesRegex(ValueError,"already instrumented"):
            patch(raw)
    def test_both_execution_paths_required(self):
        for needle in (b'P107_OLD_DYNAREC_LCOD_EXECUTED_V1',
                       b'P107_OLD_DYNAREC_INTERPRETER_FALLBACK_V2'):
            with self.assertRaisesRegex(ValueError,"JIT \\+ interpreter"):
                patch(SOURCE.replace(needle,b'none'))
    def test_only_original_flag_source(self):
        with self.assertRaisesRegex(ValueError,"lazy-flag reader include"):
            patch(SOURCE.replace(b'#    include "x86_flags.h"',b'#include "fake.h"'))
    def test_no_multiple_injection_points(self):
        with self.assertRaisesRegex(ValueError,"missing or duplicated"):
            patch(SOURCE+FUNCTION_ANCHOR.encode())
    def test_crlf_determinism(self):
        self.assertEqual(patch(SOURCE),patch(SOURCE.replace(b'\n',b'\r\n')))
if __name__=="__main__":unittest.main()
