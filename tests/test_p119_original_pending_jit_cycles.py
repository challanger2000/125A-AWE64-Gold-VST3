"""P119 original-deferred-cycle compile/runtime hook source contract negatives."""
from __future__ import annotations
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import p118_bind_runtime_jit_to_p117_opcode as p118
import p119_capture_original_pending_jit_cycles as p119
from test_p118_bind_runtime_jit_to_p117_opcode import SOURCE_SHAPED

BASE=Path(__file__).resolve().parents[1]/'third_party/86box-awe32/src/codegen'

class P119Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.acc=(BASE/'codegen_accumulate_x86-64.c').read_bytes()
        cls.jit=(BASE/'codegen_x86-64.c').read_bytes()
        assert p119.git_blob(cls.acc)==p119.ORIGINAL_ACC_SHA
        assert p119.git_blob(cls.jit)==p119.ORIGINAL_JIT_SHA
        cls.original_p118=p118.patch(cls.jit,SOURCE_SHAPED)+(
            b'\nstatic int codegen_block_ins;\n'
            b'    codegen_accumulate(ACCREG_cycles, -codegen_block_cycles);\n')
    def test_exact_original_accumulator_getter_and_jit_site(self):
        a,j=p119.patch(self.acc,self.acc,self.jit,self.original_p118)
        self.assertEqual(a.count(b'int32_t\np119_original_compiler_pending_cycles(void)'),1)
        self.assertIn(b'return acc_regs[ACCREG_cycles].count;',a)
        self.assertEqual(j.count(b'p119_original_compiler_pending_cycles()'),1)
        self.assertIn(b'addbyte(0x41); addbyte(0xb9); /* MOV R9D, imm32 */',j)
        self.assertIn(b'addbyte(0xb9); /* MOV ECX, imm32 */',j)
        self.assertIn(b'call(block, (uintptr_t)p119_x86_executed_guest_site);',j)
        self.assertIn(b'p118_x86_executed_guest_pc(pc,ebp,site);',j)
        self.assertIn(b'"Y %llu %u %08X %llu %d %d %d %d\\n"',j)
        self.assertNotIn(b'call(block, (uintptr_t)p118_x86_executed_guest_pc);',j)
    def test_no_source_mutation_and_no_flush_in_observer(self):
        a,j=p119.patch(self.acc,self.acc,self.jit,self.original_p118)
        self.assertEqual(p119.git_blob(self.acc),p119.ORIGINAL_ACC_SHA)
        self.assertEqual(p119.git_blob(self.jit),p119.ORIGINAL_JIT_SHA)
        getter=a.decode().split('p119_original_compiler_pending_cycles(void)',1)[1].split('void\ncodegen_accumulate',1)[0]
        self.assertNotIn('codegen_accumulate_flush(',getter)
        self.assertNotIn('acc_regs[ACCREG_cycles].count +=',getter)
        self.assertNotIn('update_tsc(',j.decode())
        self.assertNotIn('timer_process(',j.decode())
        self.assertNotIn('emu8k_outw(',j.decode())
    def test_missing_invalid_double_and_unbound_source(self):
        with self.assertRaisesRegex(ValueError,'source pin mismatch'):
            p119.patch(self.acc+b'\nmutated',self.acc,self.jit,self.original_p118)
        with self.assertRaisesRegex(ValueError,'requires original P118'):
            p119.patch(self.acc,self.acc,self.jit,b'broken')
        a,j=p119.patch(self.acc,self.acc,self.jit,self.original_p118)
        with self.assertRaisesRegex(ValueError,'already patched'):
            p119.patch(self.acc,a,self.jit,j)
        with self.assertRaisesRegex(ValueError,'anchor absent/duplicated'):
            # Windows Git checkouts can contain CRLF; mutate canonical LF
            # bytes, otherwise a fake negative never changes anything.
            p119.patch(self.acc,self.acc.replace(b'\r\n',b'\n').replace(
                b'void\ncodegen_accumulate(int acc_reg, int delta)\n',
                b'void\nnot_codegen_accumulate(int acc_reg, int delta)\n'),
                self.jit,self.original_p118)
        with self.assertRaisesRegex(ValueError,'requires original P118'):
            p119.patch(self.acc,self.acc,self.jit,
                self.original_p118.replace(
                    b'        addlong(p118_token);\n'
                    b'        call(block, (uintptr_t)p118_x86_executed_guest_pc);',
                    b'not a JIT argument'))

if __name__=='__main__':unittest.main()
