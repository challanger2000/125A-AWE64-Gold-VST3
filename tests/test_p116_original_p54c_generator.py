"""P116 original SHA source pins, complete timing/dependency inventory and negatives."""
from __future__ import annotations
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import p116_original_p54c_opcode_table_generator as p116

ROOT=Path(__file__).resolve().parents[1]/'third_party/86box-awe32/src/cpu'

class P116OriginalTests(unittest.TestCase):
    def test_three_pristine_source_blob_pins(self):
        for kind,name in [
            ('pentium','codegen_timing_pentium.c'),
            ('common','codegen_timing_common.c'),
            ('header','codegen_timing_common.h')]:
            original=(ROOT/name).read_bytes()
            self.assertEqual(p116.git_blob(original),p116.PINS[kind])
            with self.assertRaisesRegex(ValueError,'SHA mismatch'):
                with __import__('tempfile').TemporaryDirectory() as tmp:
                    other=Path(tmp)/'mutated.txt'
                    other.write_bytes(original+b'\nNOT ORIGINAL\n')
                    p116.pinned(other,kind)
    def test_complete_tables_compatible_sizes(self):
        src=p116.pinned(ROOT/'codegen_timing_pentium.c','pentium')
        dep=p116.pinned(ROOT/'codegen_timing_common.c','common')
        a=p116.arrays(src,'opcode_timings_p6')
        b=p116.arrays(dep,'opcode_deps')
        self.assertEqual(len(a),32)
        self.assertEqual(len(b),36)
        for k,(n,_) in a.items():
            dk=k.replace('opcode_timings_p6','opcode_deps')
            self.assertIn(dk,b)
            self.assertEqual(n,b[dk][0])
        self.assertEqual(a['opcode_timings_p6'][0],256)
        self.assertEqual(a['opcode_timings_p6_d9_mod3'][0],64)
        self.assertEqual(b['opcode_deps_0f0f'][0],256)
        with self.assertRaisesRegex(ValueError,'duplicate'):
            p116.arrays(a['opcode_timings_p6'][1]*2,'opcode_timings_p6')
    def test_original_switch_exact_source_anchor(self):
        src=p116.pinned(ROOT/'codegen_timing_pentium.c','pentium')
        selector=p116.original_switch(src)
        self.assertIn('case 0x0f:',selector)
        self.assertIn('case 0xd9:',selector)
        self.assertIn('case 0xff:',selector)
        self.assertIn('opcode_timings_p6_mod3',selector)
        with self.assertRaisesRegex(ValueError,'function anchor'):
            p116.original_switch(src.replace(
                'codegen_timing_pentium_opcode(uint8_t opcode',
                'renamed_original_opcode(uint8_t opcode'))
        with self.assertRaisesRegex(ValueError,'selection changed'):
            p116.original_switch(src.replace(
                '    switch (last_prefix) {','    switch (broken_prefix) {'))
    def test_macros_and_vector_window(self):
        src=p116.pinned(ROOT/'codegen_timing_pentium.c','pentium')
        header=p116.pinned(ROOT/'codegen_timing_common.h','header')
        self.assertIn('#define PAIR_UV',p116.macros(src,'static int       u_pipe_full;'))
        self.assertIn('#define MODRM',p116.macros(header,'extern uint64_t opcode_deps[256];'))
        self.assertEqual(len(p116.FETCH),16)
        self.assertEqual(len(p116.PREFIX),10)
        self.assertEqual(p116.PREFIX[1],0x0f)

if __name__=='__main__':
    unittest.main()
