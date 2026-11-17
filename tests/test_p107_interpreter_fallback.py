"""Safe synthetic negative controls for P107 first-time interpreted x86 execute hooks.
Never embeds the Creative driver or proprietary Win95 disk.
"""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p107_patch_old_dynarec_interpreter_fallback import (
    patch_pair,NEW_MARKER,OLD_MARKER,INTERP_DISPATCH,INTERP_INCLUDE,JIT_CALLBACK_TAIL
)

SYNTH_OLD_CPU=(
    "#include <stdint.h>\n"
    + INTERP_INCLUDE+"\n"
    + "void run_original_first_time_interpret(void) {\n"
    + INTERP_DISPATCH+"\n}\n"
).encode("ascii")
SYNTH_ALREADY_JIT=(
    "#include <stdio.h>\n"
    + "/* "+OLD_MARKER+" */\n"
    + "void callback() {\n"
    + JIT_CALLBACK_TAIL
).encode("ascii")

class OldDynarecExecutedInterpreter(unittest.TestCase):
    def test_inject_actual_dispatch_not_only_translation(self):
        cpu,code=patch_pair(SYNTH_OLD_CPU,SYNTH_ALREADY_JIT,False)
        self.assertIn(NEW_MARKER.encode(),cpu+code)
        self.assertIn(b'p107_old_dynarec_interpreter_exec(cpu_state.pc - 1u, EBP, opcode);',cpu)
        self.assertIn(b'p107_x86_executed_guest_pc(pc, actual_ebp);',code)
        self.assertIn(b'if (p107_x86_in_original_lcod(pc, opcode))',code)
        self.assertIn(b'!defined(USE_NEW_DYNAREC)',cpu)
        self.assertEqual(cpu.count(b'x86_opcodes[(opcode | cpu_state.op32) & 0x3ff](fetchdat);'),1)
    def test_original_sha_pin_cannot_be_bypassed_accidentally(self):
        with self.assertRaisesRegex(ValueError,'SHA mismatch'):
            patch_pair(SYNTH_OLD_CPU,SYNTH_ALREADY_JIT,True)
    def test_duplicate_refused(self):
        cpu,code=patch_pair(SYNTH_OLD_CPU,SYNTH_ALREADY_JIT,False)
        with self.assertRaisesRegex(ValueError,'already injected'):
            patch_pair(cpu,code,False)
    def test_wrong_interpreter_dispatch_refused(self):
        altered=SYNTH_OLD_CPU.replace(b'codegen_generate_call(',b'not_codegen_generate_call(')
        with self.assertRaisesRegex(ValueError,'exact pristine source anchor'):
            patch_pair(altered,SYNTH_ALREADY_JIT,False)
    def test_missing_jit_anchor_refused(self):
        with self.assertRaisesRegex(ValueError,'P107 old x64 source'):
            patch_pair(SYNTH_OLD_CPU,b"no marker",False)
    def test_missing_trace_tail_refused(self):
        with self.assertRaisesRegex(ValueError,'JIT executed instruction logger'):
            patch_pair(SYNTH_OLD_CPU,SYNTH_ALREADY_JIT.replace(JIT_CALLBACK_TAIL.encode(),b''),False)
    def test_crlf_determinism(self):
        a=patch_pair(SYNTH_OLD_CPU,SYNTH_ALREADY_JIT,False)
        b=patch_pair(SYNTH_OLD_CPU.replace(b'\n',b'\r\n'),
                     SYNTH_ALREADY_JIT.replace(b'\n',b'\r\n'),False)
        self.assertEqual(a,b)

if __name__=='__main__':
    unittest.main()
