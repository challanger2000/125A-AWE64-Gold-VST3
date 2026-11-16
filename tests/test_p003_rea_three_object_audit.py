"""Synthetic cross-LE-object and PC-after-I/O regression; no original VXD."""
import sys
import unittest
import struct
from pathlib import Path
from collections import Counter
from random import Random

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from p003_rea_three_object_audit import (
    load_base,external_bases,direct_call,compare_chip,parse_cpu,HELPERS,UNKNOWN,
    ANCHORS,
)

class TestThreeObjects(unittest.TestCase):
    def setUp(self):
        self.c={1:bytearray(0x6000),3:bytearray(0x600),4:bytearray(0x600)}
        self.b={1:0xC10151E4,3:0xC1177A08,4:0xC11797E4}
        self.c[1][0x4000]=0xe8
        struct.pack_into("<i",self.c[1],0x4001,HELPERS["out_word"]-0x4005)
        self.c[3][0x100]=0xe8
        self.c[4][0x120]=0xe8
        self.relocs={(3,0x100):(1,HELPERS["in_word"]),
                     (4,0x120):(1,HELPERS["out_word"])}

    def test_normal_post_io_base(self):
        e=Counter({self.b[1]+off+1:100 for off in ANCHORS})
        self.assertEqual(load_base(e),(self.b[1],1200))

    def test_real_lcod_call5(self):
        self.assertEqual(direct_call(self.c,self.relocs,self.b,
            self.b[1]+0x4005,"out_word"),(1,0x4000))

    def test_call4_rejected(self):
        self.assertIsNone(direct_call(self.c,self.relocs,self.b,
            self.b[1]+0x4004,"out_word"))

    def test_original_relocated_pnpc(self):
        self.assertEqual(direct_call(self.c,self.relocs,self.b,
            self.b[3]+0x105,"in_word"),(3,0x100))

    def test_original_relocated_pcod(self):
        self.assertEqual(direct_call(self.c,self.relocs,self.b,
            self.b[4]+0x125,"out_word"),(4,0x120))

    def test_pcod_relocation_corruption(self):
        self.relocs[(4,0x120)]=(1,HELPERS["in_word"])
        self.assertIsNone(direct_call(self.c,self.relocs,self.b,
            self.b[4]+0x125,"out_word"))

    def test_pnpc_wrong_object(self):
        self.relocs[(3,0x100)]=(4,HELPERS["in_word"])
        self.assertIsNone(direct_call(self.c,self.relocs,self.b,
            self.b[3]+0x105,"in_word"))

    def test_missing_relocation(self):
        self.assertIsNone(direct_call(self.c,{},self.b,
            self.b[4]+0x125,"out_word"))

    def test_bad_e8(self):
        self.c[3][0x100]=0x90
        self.assertIsNone(direct_call(self.c,self.relocs,self.b,
            self.b[3]+0x105,"in_word"))

    def test_unknown_return(self):
        self.assertIsNone(direct_call(self.c,self.relocs,self.b,
            UNKNOWN,"out_word"))

    def test_wrong_module_base(self):
        bad=dict(self.b)
        bad[3]+=1
        self.assertIsNone(direct_call(self.c,self.relocs,bad,
            self.b[3]+0x105,"in_word"))

    def test_unique_external_base_from_many_calls(self):
        rng=Random(125)
        s3=rng.sample(range(0x30,0x550),30)
        s4=rng.sample(range(0x30,0x550),30)
        sites={"in_word":[(3,i) for i in s3],
               "out_word":[(4,i) for i in s4]}
        returns=Counter({(self.b[o]+offset+5,helper):10
                         for helper,items in sites.items()
                         for o,offset in items})
        self.assertEqual(external_bases(returns,sites),
                         {3:self.b[3],4:self.b[4]})

    def test_ambiguous_external_base_rejected(self):
        with self.assertRaises(ValueError):
            external_bases(Counter({(0x1234,"out_word"):10}),
                           {"out_word":[(4,0x20)]})

    def test_v4_cpu_chip(self):
        row=(b"W 123 0 0028 C1018F24 0E22 0060 0 0000FFFF "
             b"C1019000 FFFFFFFF FFFFFFFF FFFFFFFF 0000 0000 0000 0000")
        ev=parse_cpu(row)
        self.assertTrue(compare_chip(ev,b"321 0E22 0060"))
        self.assertFalse(compare_chip(ev,b"321 0E22 0070"))

    def test_v3_missing_stack_refused(self):
        with self.assertRaises(ValueError):
            parse_cpu(b"W 123 0 0028 C1018F24 0E22 0060 0")

if __name__=="__main__":
    unittest.main()
