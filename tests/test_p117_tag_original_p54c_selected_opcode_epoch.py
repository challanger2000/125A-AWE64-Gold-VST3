"""P117 SHA-pinned original source-to-compiled-timing selection binding."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import p117_tag_original_p54c_selected_opcode_epoch as p117

BASE=Path(__file__).resolve().parents[1]
ORIG=(BASE/'third_party/86box-awe32/src/cpu/codegen_timing_pentium.c')

class P117Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original=ORIG.read_bytes()
    def fake_compiler(self):
        return (b'/* P111_ORIGINAL_P54C_COMPILE_TO_EXECUTE_JIT_JOIN_V1 */\n'
                b'/* P113_ORIGINAL_P54C_AGI_OPCODE_JIT_EPOCH_V1 */\n'
                b'static int codegen_block_ins;\n'
                b'    p113_capture_original_agi_opcode(block,old_pc,opcode);\n'
                b'    p110_original_p54c_translator_opcode_after();\n')
    def test_source_exact_original_anchor_binding(self):
        self.assertEqual(p117.blob(self.original),p117.PINNED_TIMING)
        timing=self.original+b'\n/* P110_ORIGINAL_P54C_TRANSLATION_TIMING_V1 */\n'
        a,b=p117.patch(self.original,timing,self.fake_compiler())
        self.assertIn(b'p117_p54c_record_selected(p117_original_opcode',a)
        self.assertIn(b'P117_ORIGINAL_P54C_SELECTED_OPCODE_JIT_EPOCH_V1',a)
        self.assertIn(b'p117_capture_original_selected(block,old_pc',b)
        self.assertEqual(a.count(b'p117_p54c_record_selected('),2)
        self.assertEqual(b.count(b'p117_capture_original_selected('),2)
        self.assertIn(b'if (!p111_join_enabled || slot>=BLOCK_SIZE',b)
        self.assertIn(b'original_prefix,fetchdat,(uint32_t)op_32',a)
        self.assertNotIn(b'update_tsc();',a)
    def test_reject_missing_probes_and_double_patching(self):
        timing=self.original+b'\n/* P110_ORIGINAL_P54C_TRANSLATION_TIMING_V1 */\n'
        a,b=p117.patch(self.original,timing,self.fake_compiler())
        with self.assertRaisesRegex(ValueError,'already instrumented'):
            p117.patch(self.original,a,b)
        with self.assertRaisesRegex(ValueError,'requires complete'):
            p117.patch(self.original,self.original,self.fake_compiler())
        with self.assertRaisesRegex(ValueError,'requires complete'):
            p117.patch(self.original,timing,b'empty missing P113')
        with self.assertRaisesRegex(ValueError,'SHA mismatch'):
            p117.patch(self.original+b'\ncorrupt',timing,self.fake_compiler())
    def test_original_selector_switch_remains_exact(self):
        timing=self.original+b'\n/* P110_ORIGINAL_P54C_TRANSLATION_TIMING_V1 */\n'
        a,_=p117.patch(self.original,timing,self.fake_compiler())
        self.assertEqual(a.count(b'    switch (last_prefix) {'),
                         self.original.count(b'    switch (last_prefix) {'))
        self.assertIn(b'opcode,timings[opcode],deps[opcode]',a)
        self.assertIn(b'    if (u_pipe_full) {',a)

if __name__=='__main__':unittest.main()
