"""Execute the REAL P110 original 86Box probe C source in a fake-state host.

This tests that the C observer actually produces the format accepted by
the Python auditor and stays disabled by default. It does NOT execute any
Win95, VxD, EMU8000 DSP, original guest x86, TSC or audio.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p110_instrument_original_p54c_compile_timing import P110_TIMING
from p110_audit_original_p54c_translation_private import audit

FAKE_P54C_CPP=(
r'''
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static uint32_t regmask_modified = 1u;
static int u_pipe_full = 1;
static int decode_delay = 2;
static int decode_delay_offset = 3;
static int codegen_block_cycles = 7;
'''
+ P110_TIMING +
r'''
int main(void)
{
    /* Only PUBLIC invented guest addresses. Same range as source study. */
    p110_original_p54c_translator_block_begin(0xC1019207u, 0x28u);
    p110_original_p54c_translator_opcode_before(0xC101920Bu, 0x28u, 0x75u);
    /* Fake a positive AGI predicate: test serialization only; original
     * actual check_agi formula remains separately SHA-locked in 86Box. */
    p110_translator_agi_calls = 1u;
    p110_translator_agi_positive = 1u;
    regmask_modified = 3u;
    u_pipe_full = 0;
    codegen_block_cycles = 9;
    p110_original_p54c_translator_opcode_after();
    return 0;
}
''')

class TestActualP110ObserverCOutput(unittest.TestCase):
    def test_host_compiled_original_probe_writes_auditable_data_and_defaults_off(self):
        compiler=shutil.which("clang") or shutil.which("gcc") or shutil.which("cc")
        self.assertIsNotNone(compiler,"need a real C compiler for probe ABI/format test")
        with tempfile.TemporaryDirectory() as tmp:
            t=Path(tmp)
            c=t/"public-fake-P110.c"
            exe=t/("public-fake-P110.exe" if os.name=="nt" else "public-fake-P110")
            trace=t/"public-fake-P110.trace"
            c.write_text(FAKE_P54C_CPP,encoding="utf8")
            build=subprocess.run([compiler,"-std=c11","-O2","-Wall","-Wextra",
                                  "-D_CRT_SECURE_NO_WARNINGS",
                                  "-Werror",str(c),"-o",str(exe)],
                                 capture_output=True,text=True,timeout=100)
            self.assertEqual(build.returncode,0,build.stderr)
            # Explicit stale environment values must not affect the OFF case.
            env=os.environ.copy()
            for name in ("AWE64_P110_TIMING_TRANSLATION_TRACE",
                         "AWE64_P110_LCOD_BASE","AWE64_P110_LCOD_SELECTOR"):
                env.pop(name,None)
            run=subprocess.run([str(exe)],env=env,capture_output=True,
                               text=True,timeout=15)
            self.assertEqual(run.returncode,0,run.stderr)
            self.assertFalse(trace.exists(),"probe is not OFF by default")
            env["AWE64_P110_TIMING_TRANSLATION_TRACE"]=str(trace)
            env["AWE64_P110_LCOD_BASE"]="0xC10151E4"
            env["AWE64_P110_LCOD_SELECTOR"]="0x28"
            run=subprocess.run([str(exe)],env=env,capture_output=True,
                               text=True,timeout=15)
            self.assertEqual(run.returncode,0,run.stderr)
            report=audit(trace,min_translations=1)
            self.assertEqual(report["translated_lcod_opcode_events"],1)
            self.assertEqual(report["original_translator_block_entries_in_lcod_window"],1)
            self.assertEqual(report["block_entries_with_carried_nonzero_register_mask"],1)
            self.assertEqual(report["actual_original_agi_positive_predicates_during_lcod_translation"],1)
            self.assertFalse(report["per_io_pentium_tsc_proven"])
            self.assertFalse(report["autonomous_vst3_pcm_proven"])
            self.assertEqual(report["guest_x86_executions_observed_in_this_trace"],0)
            raw=trace.read_text(encoding="ascii")
            self.assertIn("COMPILE_TIME_NOT_GUEST_EXECUTION",raw)
            self.assertIn("S 1 C1019207 ",raw)
            self.assertIn("O 2 C101920B 75 ",raw)
            # Wrong LCOD base must NOT write a fake positive trace.
            trace.unlink()
            env["AWE64_P110_LCOD_BASE"]="0xC1000000"
            run=subprocess.run([str(exe)],env=env,capture_output=True,
                               text=True,timeout=15)
            self.assertEqual(run.returncode,0,run.stderr)
            self.assertTrue(trace.exists())
            with self.assertRaisesRegex(ValueError,"too few"):
                audit(trace,min_translations=1)

if __name__=="__main__":
    unittest.main()
