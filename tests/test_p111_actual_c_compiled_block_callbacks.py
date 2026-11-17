"""Compile and EXECUTE actual P111 C codegen callbacks on fabricated block slots.

Runs native Windows/Linux host C with synthetic codeblock_t and synthetic TSC.
No Windows95 guest, Creative ROM/VxD, EMU8000 or VST3/PCM.
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p111_join_original_p54c_jit_compile_execute import JIT_CODE
from p111_audit_original_p54c_jit_block_join_private import audit

FAKE_COMPILER_C=(
r'''
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define BLOCK_SIZE 128
typedef struct fake_original_block {
    uint32_t pnt;
    uint32_t pc;
    uint32_t phys;
} codeblock_t;
static unsigned CS=0x28u;
static int codegen_block_cycles=8;
'''
+JIT_CODE+
r'''
int main(void) {
    codeblock_t b={7,0xC1019207u,0x2000u};
    p111_jit_compilation_start(&b);
    p111_jit_compilation_finished(&b);
    p111_old_jit_runtime_boundary(7u,b.pc,b.phys,1000u,0);
    p111_old_jit_runtime_boundary(7u,b.pc,b.phys,1031u,1);
    b.pnt=8u;
    b.phys=0x2004u;
    p111_jit_compilation_start(&b);
    p111_jit_compilation_finished(&b);  /* NEVER called in the JIT */
    b.pnt=7u;
    b.phys=0x2008u;
    p111_jit_compilation_start(&b);   /* slot 7 recycled: new epoch */
    p111_jit_compilation_finished(&b);
    p111_old_jit_runtime_boundary(7u,b.pc,0x2000u,1032u,0);
    p111_old_jit_runtime_boundary(7u,b.pc,b.phys,1040u,0);
    p111_old_jit_runtime_boundary(7u,b.pc,b.phys,1064u,1);
    return 0;
}
''')

class TestCompiledNativeP111Join(unittest.TestCase):
    def test_actual_public_c_callbacks_join_compile_with_runtime_by_generation(self):
        cc=shutil.which("clang") or shutil.which("gcc") or shutil.which("cc")
        self.assertTrue(cc,"native C compiler required for P111 source contract")
        with tempfile.TemporaryDirectory() as t:
            root=Path(t)
            src=root/"fake-p111.c"
            exe=root/("fake-p111.exe" if os.name=="nt" else "fake-p111")
            trace=root/"fake-p111.trace"
            src.write_text(FAKE_COMPILER_C,encoding="utf8")
            comp=subprocess.run(
                [cc,"-std=c11","-O2","-Wall","-Wextra","-Werror",
                 "-D_CRT_SECURE_NO_WARNINGS",str(src),"-o",str(exe)],
                text=True,capture_output=True,timeout=100)
            self.assertEqual(comp.returncode,0,comp.stderr)
            env=os.environ.copy()
            for key in ("AWE64_P111_JIT_BLOCK_JOIN_TRACE",
                        "AWE64_P111_LCOD_BASE","AWE64_P111_LCOD_SELECTOR"):
                env.pop(key,None)
            completed=subprocess.run([str(exe)],env=env,capture_output=True,
                                     text=True,timeout=15)
            self.assertEqual(completed.returncode,0,completed.stderr)
            self.assertFalse(trace.exists(),"source observer must default OFF")
            env["AWE64_P111_JIT_BLOCK_JOIN_TRACE"]=str(trace)
            env["AWE64_P111_LCOD_BASE"]="0xC10151E4"
            env["AWE64_P111_LCOD_SELECTOR"]="0x28"
            completed=subprocess.run([str(exe)],env=env,capture_output=True,
                                     text=True,timeout=15)
            self.assertEqual(completed.returncode,0,completed.stderr)
            p=audit(trace,min_runs=2)
            self.assertEqual(p["original_jit_generations"],3)
            self.assertEqual(p["original_generations_ever_executed"],2)
            self.assertEqual(p["original_compiled_but_unexecuted"],1)
            self.assertEqual(p["real_code_block_invocations"],2)
            self.assertEqual(p["real_code_block_returns"],2)
            self.assertEqual(p["runtime_block_boundary_tsc_delta_sum"],55)
            self.assertFalse(p["per_instruction_cpu_tsc_proven"])
            self.assertFalse(p["autonomous_vst3_pcm_parity_proven"])
            # Even if the executor is called for slot 7 with an old
            # physical address, no stale generation should appear.
            source=trace.read_bytes()
            self.assertIn(b"F 2 1 7 C1019207 00002000",source)
            self.assertNotIn(b"00002000 1032",source)
            self.assertIn(b"R 9 3 7 C1019207 00002008 1040",source)
            trace.unlink()
            env["AWE64_P111_LCOD_BASE"]="0xC1000000"
            completed=subprocess.run([str(exe)],env=env,capture_output=True,
                                     text=True,timeout=15)
            self.assertEqual(completed.returncode,0,completed.stderr)
            self.assertTrue(trace.exists())
            with self.assertRaisesRegex(ValueError,"insufficient"):
                audit(trace,min_runs=1)

if __name__=="__main__":unittest.main()
