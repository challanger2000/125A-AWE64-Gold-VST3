"""P115: source-contract, mutation and compiled callback semantics (fictional)."""
from __future__ import annotations
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import p115_original_live_cpu_tsc_probe as probe

DYN=(probe.ANCHOR_CPU+'\nvoid update_tsc(void) {}\n').encode()
IO=(f'/* {probe.P003_IO_MARKER} */\n'
    'static FILE *p003_cpu_io_file;\n'
    'static unsigned long long p003_cpu_io_seq;\n'
    +probe.ANCHOR_IO+'{\n if(p003_cpu_io_file) p003_cpu_io_seq++;\n'
    +probe.END_IO+' trailing original notes */\n').encode()

class P115Tests(unittest.TestCase):
    def setUp(self):
        self.old=probe.ORIGINAL_DYNAREC_BLOB
        probe.ORIGINAL_DYNAREC_BLOB=probe.git_blob(DYN)
    def tearDown(self):
        probe.ORIGINAL_DYNAREC_BLOB=self.old
    def test_source_patch_only(self):
        dyn,io=probe.patch(DYN,DYN,IO)
        self.assertIn(b'p115_original_dynarec_live_tsc',dyn)
        self.assertIn(b'P115_ORIGINAL_DYNAREC_LIVE_CPU_TSC_V1',io)
        self.assertIn(b'p115_original_cpu_port(direction,port,p003_cpu_io_seq-1u)',io)
        self.assertIn(b'if (!p003_cpu_io_file) return;',io)
        self.assertIn(b'if (!cpu_use_dynarec) return 0;',dyn)
        self.assertNotIn(b'update_tsc();',dyn)
        self.assertEqual(io.count(b'p003_cpu_io_seq++'),1)
        self.assertEqual(dyn.count(b'void update_tsc(void) {}'),1)
    def test_sha_precondition(self):
        probe.ORIGINAL_DYNAREC_BLOB=self.old
        with self.assertRaisesRegex(ValueError,'SHA-pinned'):
            probe.patch(DYN,DYN,IO)
    def test_no_double_patch_and_missing_p003(self):
        dyn,io=probe.patch(DYN,DYN,IO)
        with self.assertRaisesRegex(ValueError,'already instrumented'):
            probe.patch(DYN,dyn,io)
        with self.assertRaisesRegex(ValueError,'previously P003'):
            probe.patch(DYN,DYN,b'not an instrumented io')
        with self.assertRaisesRegex(ValueError,'missing/duplicate'):
            probe.patch(DYN,DYN,IO.replace(probe.END_IO.encode(),b''))
    def test_compiled_original_function_arithmetic(self):
        cc=os.environ.get('CC','cc')
        if shutil.which(cc) is None:
            self.fail('P115 no C compiler available: '+cc)
        check=r'''
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
static int cpu_use_dynarec=1;
static int32_t cycles_old=1000, cycles=996;
static uint64_t tsc_old=100,tsc=100;
static int inrecomp=1;
#ifdef USE_ACYCS
static int32_t acycs=3;
#endif
#define EXPECT(cond) do {if(!(cond)) return __LINE__;} while(0)
'''+probe.CPU_PROBE+r'''
int main(void) {
    uint64_t effective=0,raw=0,old=0;
    int32_t at=0,rem=0,aux=0;
    int jit=0;
    EXPECT(p115_original_dynarec_live_tsc(&effective,&raw,&old,&at,&rem,&aux,&jit)==1);
#ifdef USE_ACYCS
    EXPECT(effective==107 && aux==3);
#else
    EXPECT(effective==104 && aux==0);
#endif
    EXPECT(old==100 && at==1000 && rem==996 && jit==1);
    tsc=110;
    EXPECT(p115_original_dynarec_live_tsc(&effective,&raw,&old,&at,&rem,&aux,&jit)==1);
    EXPECT(effective==110);
    tsc=100; cycles=996; inrecomp=0;
    EXPECT(p115_original_dynarec_live_tsc(&effective,&raw,&old,&at,&rem,&aux,&jit)==1);
    EXPECT(effective==104 && aux==0 && jit==0);
    tsc=99;
    EXPECT(p115_original_dynarec_live_tsc(&effective,&raw,&old,&at,&rem,&aux,&jit)==-1);
    tsc=UINT64_MAX-1;tsc_old=UINT64_MAX-1;cycles=990;
    EXPECT(p115_original_dynarec_live_tsc(&effective,&raw,&old,&at,&rem,&aux,&jit)==-1);
    cpu_use_dynarec=0;
    EXPECT(p115_original_dynarec_live_tsc(&effective,&raw,&old,&at,&rem,&aux,&jit)==0);
    EXPECT(p115_original_dynarec_live_tsc(0,&raw,&old,&at,&rem,&aux,&jit)==-1);
    puts("P115_ORIGINAL_LIVE_TSC_CALLBACK_C_TEST_PASS");
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as t:
            src=Path(t)/'live.c';src.write_text(check)
            exe=Path(t)/('live.exe' if os.name=='nt' else 'live')
            for flags in [[],['-DUSE_ACYCS=1']]:
                compile=subprocess.run([cc,'-std=c11','-O2','-Wall','-Wextra','-Werror',*flags,str(src),'-o',str(exe)],capture_output=True,text=True)
                self.assertEqual(compile.returncode,0,compile.stdout+compile.stderr)
                ran=subprocess.run([str(exe)],capture_output=True,text=True)
                self.assertEqual(ran.returncode,0,ran.stdout+ran.stderr)
                self.assertIn('P115_ORIGINAL_LIVE_TSC_CALLBACK_C_TEST_PASS',ran.stdout)

if __name__=='__main__':unittest.main()
