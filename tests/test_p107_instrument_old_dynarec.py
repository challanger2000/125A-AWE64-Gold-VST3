"""P107 source-owned NEGATIVE controls for the pinned old 86Box Dynarec.

No Creative VxD, original ROM, bank, Win95 guest, trace or PCM is used.
"""
import sys
from pathlib import Path
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p107_instrument_old_dynarec import (
    patch,git_blob_sha1,MARKER,INCLUDE_ANCHOR,GENERATE_ANCHOR
)

SOURCE_SHAPED=(
    "#if defined __amd64__ || defined _M_X64\n"
    "#    include <stdlib.h>\n"
    +INCLUDE_ANCHOR+"\n"
    + "void block_end() {\n"
      "    codegen_accumulate(ACCREG_cycles, -codegen_block_cycles);\n"
      "}\n"
      "void\ncodegen_generate_call(uint8_t opcode, OpFn op, uint32_t fetchdat, "
      "uint32_t new_pc, uint32_t old_pc)\n{\n"
    +GENERATE_ANCHOR+"\n"
      "        return;\n"
      "    }\n}\n"
    ).encode("utf-8")


class ProbeContract(unittest.TestCase):
    def test_strict_source_only_branch_observer(self):
        patched=patch(SOURCE_SHAPED,git_blob_sha1(SOURCE_SHAPED))
        self.assertIn(MARKER.encode(),patched)
        self.assertIn(b"AWE64_P107_EXECUTED_X86_TRACE",patched)
        self.assertIn(b"AWE64_P107_LCOD_BASE",patched)
        self.assertIn(b"load_param_1_32(block, old_pc)",patched)
        self.assertIn(b"load_param_2_reg_32(LOAD_REG_L(REG_EBP))",patched)
        self.assertIn(b"call(block, (uintptr_t)p107_x86_executed_guest_pc)",patched)
        self.assertIn(b"if (p107_x86_in_original_lcod(old_pc, opcode))",patched)
        self.assertIn(b"guest_instruction_pc",patched)
        self.assertIn(b"p107_guest_stack_minus18",patched)
        self.assertIn(b"readlookup2[linear >> 12]",patched)
        self.assertNotIn(b"writemem",patched.lower())
        self.assertNotIn(b"emu8k_",patched.lower())
        self.assertEqual(patched.count(b"codegen_block_cycles = 0;"),1)
        self.assertEqual(patched.count(b"codegen_accumulate(ACCREG_cycles, -codegen_block_cycles)"),2)
        self.assertEqual(patched.count(b"p107_x86_executed_guest_pc);"),1)

    def test_original_blob_pin_rejects_synthetic_and_wrong_fork(self):
        with self.assertRaisesRegex(ValueError,"Pinned upstream OLD dynarec"):
            patch(SOURCE_SHAPED)

    def test_original_byte_mutation_fails(self):
        original_hash=git_blob_sha1(SOURCE_SHAPED)
        mutated=SOURCE_SHAPED+b"\n/* foreign edit */"
        with self.assertRaisesRegex(ValueError,"hash mismatch"):
            patch(mutated,original_hash)

    def test_reinjection_fails(self):
        compiled=patch(SOURCE_SHAPED,git_blob_sha1(SOURCE_SHAPED))
        with self.assertRaisesRegex(ValueError,"Already instrumented"):
            patch(compiled,None)

    def test_wrong_new_dynarec_source_rejected(self):
        changed=SOURCE_SHAPED+b"\n#define USE_NEW_DYNAREC 1\n"
        with self.assertRaisesRegex(ValueError,"Unexpected NEW-DYNAREC"):
            patch(changed,None)

    def test_missing_generator_anchor_rejected(self):
        altered=SOURCE_SHAPED.replace(GENERATE_ANCHOR.encode(),
                                     b"/* original timing hook absent */")
        with self.assertRaisesRegex(ValueError,"original generator timing"):
            patch(altered,None)

    def test_missing_include_rejected(self):
        altered=SOURCE_SHAPED.replace(INCLUDE_ANCHOR.encode(),b"/* none */")
        with self.assertRaisesRegex(ValueError,"old Dynarec header"):
            patch(altered,None)

    def test_crlf_still_same_pinned_blob(self):
        crlf=SOURCE_SHAPED.replace(b"\n",b"\r\n")
        self.assertEqual(git_blob_sha1(crlf),git_blob_sha1(SOURCE_SHAPED))
        self.assertEqual(patch(crlf,git_blob_sha1(SOURCE_SHAPED)),
                         patch(SOURCE_SHAPED,git_blob_sha1(SOURCE_SHAPED)))


if __name__=="__main__":
    unittest.main()
