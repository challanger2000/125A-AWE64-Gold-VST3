"""P110 original 86Box timing translator patch: synthetic + SHA-pinned guards.

All fixtures are fake. CI also verifies actual public original source blobs
through the existing real 86Box fork checkout; no Win95 ROM/driver/audio.
"""
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p110_instrument_original_p54c_compile_timing import (
    patch_pair,blob,MARKER,P107,P108,P110_TIMING
)

TIMING=(
    '#include <stdio.h>\n'
    'static uint32_t regmask_modified;\n'
    'static int u_pipe_full;\nstatic int decode_delay;\n'
    'static int decode_delay_offset;\n'
    'static int     prefixes;\n'
    'int check_agi(void) {\n'
    '    return (regmask_modified & addr_regmask) & ~REGMASK_IMPL_ESP;\n'
    '}\n'
).encode("utf8")
CODEGEN=(
    f'/* {P107} */\n/* {P108} */\n'
    'void\ncodegen_block_start_recompile(codeblock_t *block)\n'
    '{\n    codegen_timing_block_start();\n}\n'
    'void fake_old_codegen(void)\n{\n'
    '    codegen_timing_opcode(opcode, fetchdat, op_32, op_pc);\n}\n'
).encode("utf8")

class TestP110OriginalPentiumTranslationTimeOnly(unittest.TestCase):
    def test_patch_only_original_pentium_timing_and_old_x64_codegen(self):
        timing,code=patch_pair(TIMING,CODEGEN,check_original_sha=False)
        self.assertIn(MARKER.encode(),timing)
        self.assertIn(b'p110_original_p54c_translator_opcode_before',code)
        self.assertIn(b'p110_original_p54c_translator_opcode_after();',code)
        self.assertIn(b'p110_original_p54c_translator_block_begin',code)
        self.assertIn(b'(unsigned)regmask_modified',timing)
        self.assertIn(b'p110_translator_agi_positive',timing)
        self.assertIn(b'COMPILE_TIME_NOT_GUEST_EXECUTION',timing)
        self.assertIn(b'AWE64_P110_TIMING_TRANSLATION_TRACE',timing)
        self.assertIn(b'AWE64_P110_LCOD_SELECTOR',timing)
        self.assertIn(b'0x4023u',timing)
        self.assertIn(b'0x4125u',timing)
        self.assertNotIn(b'emu8k_',timing.lower())
    def test_no_original_pentium_timestamp_claims(self):
        timing,code=patch_pair(TIMING,CODEGEN,check_original_sha=False)
        self.assertIn(b'block_timing_NOT_instructions_tsc',timing)
        self.assertIn(b'no_tsc_parity',Path(__file__).resolve().parents[1].joinpath(
            'tools/p110_instrument_original_p54c_compile_timing.py'
        ).read_bytes())
    def test_original_git_blob_required(self):
        with self.assertRaisesRegex(ValueError,'SHA pin mismatch'):
            patch_pair(TIMING,CODEGEN)
    def test_refuse_nonoriginal_old_codegen(self):
        for marker in (P107,P108):
            with self.subTest(marker=marker):
                with self.assertRaisesRegex(ValueError,'P107/P108'):
                    patch_pair(TIMING,CODEGEN.replace(marker.encode(),b'INVALID'),
                               check_original_sha=False)
    def test_no_ambiguous_source_injection(self):
        with self.assertRaisesRegex(ValueError,'anchor'):
            patch_pair(TIMING+TIMING,CODEGEN,check_original_sha=False)
    def test_no_repatch(self):
        timing,code=patch_pair(TIMING,CODEGEN,check_original_sha=False)
        with self.assertRaisesRegex(ValueError,'already patched'):
            patch_pair(timing,code,check_original_sha=False)
    def test_source_line_endings_deterministic(self):
        a,b=patch_pair(TIMING,CODEGEN,check_original_sha=False)
        c,d=patch_pair(TIMING.replace(b'\n',b'\r\n'),
                       CODEGEN.replace(b'\n',b'\r\n'),
                       check_original_sha=False)
        self.assertEqual((a,b),(c,d))
    def test_no_timing_callback_when_probe_disabled_by_default(self):
        self.assertIn('if (!path || !*path || !base || !*base || !selector || !*selector)return;',P110_TIMING)
        self.assertIn('p110_translator_active = 0;',P110_TIMING)
        self.assertIn('if (!p110_translator_active || !p110_translator_file)return;',P110_TIMING)
    def test_source_check_reads_original_agi_result_once(self):
        timing,_=patch_pair(TIMING,CODEGEN,check_original_sha=False)
        self.assertEqual(timing.count(
            b'uint32_t original_agi = (regmask_modified & addr_regmask) & ~REGMASK_IMPL_ESP;'),1)
        self.assertEqual(timing.count(b'return original_agi;'),1)
        self.assertIn(b'if (original_agi != 0u)++p110_translator_agi_positive;',timing)

if __name__=="__main__":unittest.main()
