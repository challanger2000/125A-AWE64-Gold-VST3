"""Original 86Box P111 old-x64 JIT translation/runtime join, synthetic only."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p111_join_original_p54c_jit_compile_execute import (
    patch_pair,blob,JIT_CODE,P111_MARKER
)

FAKE_CPU=b'''#include "386_common.h"
/* P107_OLD_DYNAREC_INTERPRETER_FALLBACK_V2 */
extern void p107_old_dynarec_interpreter_exec(void);
void sample(void){
        inrecomp = 1;
        code();
#    ifdef USE_ACYCS
}
'''
FAKE_JIT=b'''/* P107_OLD_DYNAREC_INTERPRETER_FALLBACK_V2 */
/* P108_DIRECT_EXECUTED_X86_JCC_CF_ZF_V1 */
/* P110_ORIGINAL_P54C_TRANSLATION_TIMING_V1 */
int        block_pos;
void sample(void) {
    p110_original_p54c_translator_block_begin(cpu_state.pc, (uint16_t)CS);
    codegen_timing_block_start();
    codegen_timing_block_end();
    codegen_accumulate(ACCREG_cycles, -codegen_block_cycles);
}
'''
class TestP111OriginalJitGenerationJoin(unittest.TestCase):
    def test_compilation_and_runtime_hooks_are_in_original_source_locations(self):
        cpu,code=patch_pair(b"fictional original cpu",FAKE_CPU,FAKE_JIT,
                            original_sha_check=False)
        self.assertEqual(cpu.count(b'p111_old_jit_runtime_boundary('),3)
        self.assertIn(b'block->pnt,block->pc',cpu)
        self.assertIn(b'block->phys,tsc,0',cpu)
        self.assertIn(b'block->phys,tsc,1',cpu)
        self.assertIn(b'p111_jit_compilation_start(block);',code)
        self.assertIn(b'p111_jit_compilation_finished(block);',code)
        self.assertIn(b'codegen_timing_block_end();\n    p111_jit_compilation_finished(block);',code)
        self.assertIn(b'P111_ORIGINAL_P54C_COMPILE_TO_EXECUTE_JIT_JOIN_V1',code)
    def test_reject_second_injection(self):
        cpu,code=patch_pair(b"synthetic",FAKE_CPU,FAKE_JIT,
                            original_sha_check=False)
        with self.assertRaisesRegex(ValueError,"already present"):
            patch_pair(b"synthetic",cpu,code,original_sha_check=False)
    def test_wrong_original_386_blob_blocked(self):
        with self.assertRaisesRegex(ValueError,"SHA pin mismatch"):
            patch_pair(b"synthetic",FAKE_CPU,FAKE_JIT)
    def test_no_p107_or_p110_original_instrumentation_rejected(self):
        for src,guest in ((FAKE_CPU.replace(
              b'p107_old_dynarec_interpreter_exec',b'bad'),FAKE_JIT),
              (FAKE_CPU,FAKE_JIT.replace(
              b'P110_ORIGINAL_P54C_TRANSLATION_TIMING_V1',b'bad')),
              (FAKE_CPU,FAKE_JIT.replace(
              b'P108_DIRECT_EXECUTED_X86_JCC_CF_ZF_V1',b'bad'))):
            with self.assertRaisesRegex(ValueError,"requires"):
                patch_pair(b"synthetic",src,guest,original_sha_check=False)
    def test_duplicate_code_exec_site_rejected(self):
        with self.assertRaisesRegex(ValueError,"anchor"):
            patch_pair(b"fictional",FAKE_CPU+FAKE_CPU,FAKE_JIT,
                       original_sha_check=False)
    def test_exact_separate_compiled_epoch_and_actual_execution_trace(self):
        self.assertIn("p111_compile_epoch",JIT_CODE)
        self.assertIn("p111_epoch_by_slot[BLOCK_SIZE]",JIT_CODE)
        self.assertIn("p111_compiled_by_slot[BLOCK_SIZE]",JIT_CODE)
        self.assertIn("p111_phys_by_slot[BLOCK_SIZE]",JIT_CODE)
        self.assertIn("p111_compiled_by_slot[slot]=0u;",JIT_CODE)
        self.assertIn("p111_compiled_by_slot[slot]=1u;",JIT_CODE)
        self.assertIn('on_exit ? \'D\':\'R\'',JIT_CODE)
        self.assertIn("AWE64_P111_JIT_BLOCK_JOIN_TRACE",JIT_CODE)
        self.assertIn("RUNTIME_BLOCK_NOT_INSTRUCTION_TSC",JIT_CODE)
        self.assertIn("P111_EVENT_CAP_REACHED_NO_COMPLETE_CLAIM",JIT_CODE)
        self.assertNotIn("emu8k_",JIT_CODE.lower())
    def test_original_src_line_endings_normalized(self):
        a,b=patch_pair(b"fake",FAKE_CPU,FAKE_JIT,False)
        c,d=patch_pair(b"fake",FAKE_CPU.replace(b"\n",b"\r\n"),
                       FAKE_JIT.replace(b"\n",b"\r\n"),False)
        self.assertEqual((a,b),(c,d))

if __name__=="__main__":unittest.main()
