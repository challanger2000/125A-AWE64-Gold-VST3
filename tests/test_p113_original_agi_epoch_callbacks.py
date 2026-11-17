"""P113 real C callback/trace ABI against fictional guest state, Win/Linux.

Uses exact injected original P110/P111/P113 OBSERVER code, never Creative
audio, ROM, Win95, emulator guest x86, or plugin DSP.
"""
from __future__ import annotations
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p110_instrument_original_p54c_compile_timing import P110_TIMING
from p111_join_original_p54c_jit_compile_execute import JIT_CODE
from p113_tag_original_p110_opcode_agi_with_p111_compile_epoch import (
    P113_JIT,P113_TIMING_GETTER,patch_pair
)
from p113_audit_original_opcode_agi_compilation_epoch_private import audit

FAKE_C=(
r'''
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define BLOCK_SIZE 128
typedef struct public_fake_codeblock {
    uint32_t pnt;
    uint32_t pc;
    uint32_t phys;
} codeblock_t;
static unsigned CS=0x28u;
static uint32_t regmask_modified=1u;
static int u_pipe_full=0;
static int decode_delay=0;
static int decode_delay_offset=0;
static int codegen_block_cycles=7;
'''
+P110_TIMING+P113_TIMING_GETTER+JIT_CODE+P113_JIT+
r'''
static void fictional_original_shape_compile(unsigned slot,unsigned phys,
                                             unsigned positive,int execute)
{
    codeblock_t block={slot,0xC1019207u,phys};
    const uint32_t op_pc=0xC101920Bu;
    p111_jit_compilation_start(&block);
    p110_original_p54c_translator_block_begin(block.pc,(uint16_t)CS);
    p110_original_p54c_translator_opcode_before(op_pc,(uint16_t)CS,0x75u);
    /* This is fake data for the ABI test, not actual AGI or guest CPU. */
    p110_translator_agi_calls=1u;
    p110_translator_agi_positive=positive;
    p113_capture_original_agi_opcode(&block,op_pc,0x75u);
    p110_original_p54c_translator_opcode_after();
    p111_jit_compilation_finished(&block);
    if (execute) {
        p111_old_jit_runtime_boundary(slot,block.pc,block.phys,100u,0);
        p111_old_jit_runtime_boundary(slot,block.pc,block.phys,107u,1);
    }
}
int main(void)
{
    fictional_original_shape_compile(7u,0x2000u,1u,1);
    fictional_original_shape_compile(8u,0x2004u,0u,0);
    return 0;
}
''')

class TestP113OriginalEpochNativeC(unittest.TestCase):
    def test_source_patcher_only_recognizes_original_p110_p111_hooks(self):
        mock_timing=(P110_TIMING+
                      "\nvoid p110_original_p54c_translator_opcode_before(void){}\n").encode()
        mock_jit=(
          "/* P111_ORIGINAL_P54C_COMPILE_TO_EXECUTE_JIT_JOIN_V1 */\n"
          "int block_pos;\nstatic int codegen_block_ins;\n"
          "void p111_jit_compilation_start(codeblock_t *block){}\n"
          "    p111_jit_compilation_start(block);\n"
          "void p110_original_p54c_translator_opcode_before(void){}\n"
          "    p110_original_p54c_translator_opcode_after();\n"
        ).encode()
        a,b=patch_pair(mock_timing,mock_jit)
        self.assertIn(b"P113_ORIGINAL_P54C_AGI_OPCODE_JIT_EPOCH_V1",a)
        self.assertIn(b"p113_capture_original_agi_opcode(block,old_pc,opcode);",b)
        self.assertIn(b"p113_original_p54c_get_last_opcode_agi",a)
        with self.assertRaisesRegex(ValueError,"already patched"):
            patch_pair(a,b)
        with self.assertRaisesRegex(ValueError,"requires complete original P110 and P111"):
            patch_pair(P110_TIMING.encode(),b"wrong")
    def test_native_c_real_source_emit_exact_epoch_and_original_counter_format(self):
        cc=shutil.which("clang") or shutil.which("gcc") or shutil.which("cc")
        self.assertTrue(cc,"native C compiler required for source callback ABI")
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            c=root/"public-p113-abi.c"
            exe=root/("public-p113-abi.exe" if os.name=="nt" else "public-p113-abi")
            trace110=root/"P110-FAKE"
            trace111=root/"P111-FAKE"
            trace113=root/"P113-FAKE"
            c.write_text(FAKE_C,encoding="utf8")
            built=subprocess.run([cc,"-std=c11","-O2","-Wall","-Wextra","-Werror",
                "-D_CRT_SECURE_NO_WARNINGS",str(c),"-o",str(exe)],
                capture_output=True,text=True,timeout=100)
            self.assertEqual(built.returncode,0,built.stderr)
            env=os.environ.copy()
            for p in ("P110_TIMING_TRANSLATION_TRACE","P110_LCOD_BASE",
                      "P110_LCOD_SELECTOR","P111_JIT_BLOCK_JOIN_TRACE",
                      "P111_LCOD_BASE","P111_LCOD_SELECTOR",
                      "P113_ORIGINAL_AGI_EPOCH_TRACE","P113_LCOD_BASE",
                      "P113_LCOD_SELECTOR"):
                env.pop("AWE64_"+p,None)
            run=subprocess.run([str(exe)],env=env,capture_output=True,
                               text=True,timeout=15)
            self.assertEqual(run.returncode,0,run.stderr)
            for trace in (trace110,trace111,trace113):
                self.assertFalse(trace.exists(),"original probe must default OFF")
            prefixes=(
                ("AWE64_P110_",trace110,"TIMING_TRANSLATION_TRACE"),
                ("AWE64_P111_",trace111,"JIT_BLOCK_JOIN_TRACE"),
                ("AWE64_P113_",trace113,"ORIGINAL_AGI_EPOCH_TRACE"))
            for prefix,path,flag in prefixes:
                env[prefix+flag]=str(path)
                env[prefix+"LCOD_BASE"]="0xC10151E4"
                env[prefix+"LCOD_SELECTOR"]="0x28"
            run=subprocess.run([str(exe)],env=env,capture_output=True,
                               text=True,timeout=15)
            self.assertEqual(run.returncode,0,run.stderr)
            for trace in (trace110,trace111,trace113):
                self.assertTrue(trace.is_file(),"original observer did not write")
            r=audit(trace110,trace111,trace113,min_agis=1)
            self.assertEqual(r["original_opcode_agi_events_generation_matched"],2)
            self.assertEqual(r["original_positive_agi_checks_in_jit_executed_generations"],1)
            self.assertEqual(r["original_opcode_events_in_only_compiled_generations"],1)
            self.assertFalse(r["per_guest_instruction_tsc_observed"])
            self.assertFalse(r["original_91_93_95_cpu_cycle_causality_proven"])
            raw=trace113.read_text(encoding="ascii")
            self.assertIn("E 1 1 C101920B 75 1 1",raw)
            self.assertIn("E 2 2 C101920B 75 1 0",raw)
            # A different, reused original compiler epoch with unchanged
            # guest PC must be rejected if it claims the previous epoch.
            mutated=raw.replace("E 2 2 ","E 2 1 ")
            trace113.write_text(mutated,encoding="ascii")
            with self.assertRaisesRegex(ValueError,"compilation ordinal"):
                audit(trace110,trace111,trace113,min_agis=0)

if __name__=="__main__":
    unittest.main()
