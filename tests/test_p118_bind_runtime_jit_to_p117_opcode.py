"""P118 source-only original old-x64 JIT-site token and mutation checks."""
from __future__ import annotations
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import p118_bind_runtime_jit_to_p117_opcode as p118

PRISTINE=Path(__file__).resolve().parents[1]/'third_party/86box-awe32/src/codegen/codegen_x86-64.c'

# Source-shaped fixture: original inputs + exact P107/P111/P117 anchors;
# no Win95/VxD/ROM/PCs used. The real original fork is separately compiled CI.
SOURCE_SHAPED=(
    '// P107_OLD_DYNAREC_LCOD_EXECUTED_V1\n'
    '// P111_ORIGINAL_P54C_COMPILE_TO_EXECUTE_JIT_JOIN_V1\n'
    '// P117_ORIGINAL_P54C_SELECTED_OPCODE_JIT_EPOCH_V1\n'
    'static uint64_t p117_selected_seq;\n'
    'void p117_capture(void) {\n'
    '    p117_decoded_word word;\n'
    '    uint32_t off,slot;\n'
    '    epoch=p111_epoch_by_slot[slot];\n'
    '    if (++p117_selected_seq>500000ULL) return;\n'
    '    fprintf(p117_selected_file,"D ...");\n'
    '}\n'
    'void codegen_generate_call(void) {\n'
    '    p117_capture_original_selected(block,old_pc,opcode,fetchdat,op_32);\n'
    '    p110_original_p54c_translator_opcode_after();\n'
    '    if (p107_x86_in_original_lcod(old_pc, opcode)) {\n'
    '        load_param_1_32(block, old_pc);\n'
    '        load_param_2_reg_32(LOAD_REG_L(REG_EBP));\n'
    '        call(block, (uintptr_t)p107_x86_executed_guest_pc);\n'
    '    }\n'
    '}\n'
).encode()

class TestOriginalJitSiteBinding(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pristine=PRISTINE.read_bytes()
        assert p118.git_blob_sha1(cls.pristine)==p118.ORIGINAL_CODEGEN_SHA
    def test_abi_and_compile_epoch_site_embed(self):
        patched=p118.patch(self.pristine,SOURCE_SHAPED).decode('utf8')
        self.assertIn('p118_last_p117_site=(uint32_t)p117_selected_seq',patched)
        self.assertIn('p118_last_p117_site=0u',patched)
        self.assertIn('p118_last_p117_epoch!=p111_epoch_by_slot[slot]',patched)
        self.assertIn('p118_last_p117_pc!=pc',patched)
        self.assertIn('addbyte(0x41); addbyte(0xb8); /* MOV R8D, imm32 */',patched)
        self.assertIn('addbyte(0xba); /* MOV EDX, imm32 */',patched)
        self.assertIn('call(block, (uintptr_t)p118_x86_executed_guest_pc)',patched)
        self.assertNotIn('call(block, (uintptr_t)p107_x86_executed_guest_pc)',patched)
        self.assertIn('p107_x86_executed_guest_pc(pc,ebp)',patched)
        self.assertIn('p107_x86_trace_seq-1u',patched)
        self.assertIn('P118_SOURCE_PROVENANCE_MISSING_NO_GO',patched)
        self.assertEqual(patched.count('p118_x86_executed_guest_pc(uint32_t'),1)
        self.assertLess(
            patched.index('p117_capture_original_selected(block,old_pc'),
            patched.index('p118_source_site_token(block,old_pc);',patched.index('void codegen_generate_call')))
    def test_fail_closed_double_patch_and_missing_probes(self):
        patched=p118.patch(self.pristine,SOURCE_SHAPED)
        with self.assertRaisesRegex(ValueError,'already instrumented'):
            p118.patch(self.pristine,patched)
        with self.assertRaisesRegex(ValueError,'requires the original'):
            p118.patch(self.pristine,b'empty')
        with self.assertRaisesRegex(ValueError,'SHA pin'):
            p118.patch(self.pristine+b'\ncorrupt',SOURCE_SHAPED)
        with self.assertRaisesRegex(ValueError,'anchor absent/duplicated'):
            p118.patch(self.pristine,SOURCE_SHAPED.replace(
                b'static uint64_t p117_selected_seq;',
                b''))
        with self.assertRaisesRegex(ValueError,'anchor absent/duplicated'):
            p118.patch(self.pristine,SOURCE_SHAPED.replace(
                b'    p117_capture_original_selected(block,old_pc,opcode,fetchdat,op_32);\n',
                b'    p117_capture_original_selected(block,old_pc,opcode,fetchdat,op_32);\n'*2))
    def test_no_guest_dsp_or_cpu_mutation(self):
        patched=p118.patch(self.pristine,SOURCE_SHAPED).decode('utf8')
        self.assertNotIn('update_tsc();',patched)
        self.assertNotIn('emu8k_outw(',patched)
        self.assertNotIn('timer_process();',patched)
        self.assertNotIn('set_acycs(',patched)
        self.assertIn('AWE64_P118_EXECUTED_SITE_TRACE',patched)
        self.assertNotIn('SYNTHGM',patched)

if __name__=='__main__':unittest.main()
