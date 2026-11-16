"""Synthetic compile-and-run check: I/O helper EBP != allocator EBP.

Tests the exact injected C functions, with fake mapped guest RAM. No Creative
binaries, samples, ROM, or private trace bytes are committed.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from instrument_p003_v5_voice_rows import READER

C = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
static uint8_t ram[0x20000];
static uintptr_t readlookup2[128];
struct {struct {uint32_t base;} seg_ds,seg_ss;} cpu_state;
static uint32_t EBP=0x14000, EBX=0x17000;
static unsigned long long tsc=123456789;
static FILE *p003_cpu_io_file;
static int p003_guest_callstack_enabled=1;
static void put32(unsigned p,uint32_t v){memcpy(ram+p,&v,4);}
__READER__
int main(void) {
 for(int i=0;i<128;i++)readlookup2[i]=(uintptr_t)ram;
 put32(0x14000,0x14200); /* [helper EBP] = allocator EBP */
 put32(0x13ffc,0x16000); /* [helper EBP-4] = caller ESI, NOT score */
 put32(0x141fc,0x567); /* [allocator EBP-4] */
 put32(0x141f4,0x1234); /* [allocator EBP-12] */
 put32(0x141ec,0x4567); /* [allocator EBP-20] */
 put32(0x16018,3); /* [saved ESI+0x18] */
 ram[0x16fed]=0x1a; ram[0x1700d]=0x07;
 p003_cpu_io_file=tmpfile();
 if(!p003_cpu_io_file)return 1;
 p003_v5_guest_caller('R',0x0a22,0xc10192a8,33);
 rewind(p003_cpu_io_file);
 char line[512];
 if(!fgets(line,sizeof(line),p003_cpu_io_file))return 2;
 unsigned long long seq=0,t=0;
 unsigned ret=0,ebx=0,helper=0,caller=0,esi=0,mask=0;
 unsigned curr=0,prev=0,best=0,mode=0,n0=0,n2=0;
 int n=sscanf(line,"# P003_V5_CALLER_FRAME %llu %llu %x %x %x %x %x %x %x %x %x %x %x %x",
    &seq,&t,&ret,&ebx,&helper,&caller,&esi,&mask,&curr,&prev,&best,&mode,&n0,&n2);
 if(n!=14||seq!=33||t!=123456789||ret!=0xc10192a8||
    ebx!=EBX||helper!=EBP||caller!=0x14200||esi!=0x16000||
    mask!=255||curr!=0x567||prev!=0x1234||best!=0x4567||
    mode!=3||n0!=0x1a||n2!=0x07)return 3;
 p003_v5_guest_row('R',0x0a22,0xc10192a8,33);
 fclose(p003_cpu_io_file);
 return 0;
}
'''

class TestCallerFrame(unittest.TestCase):
 def test_saved_caller_ebp_not_helper_ebp(self):
  with tempfile.TemporaryDirectory() as d:
   source=Path(d)/'source.c'
   exe=Path(d)/('test.exe' if os.name=='nt' else 'test')
   source.write_text(C.replace('__READER__',READER))
   compiler=os.environ.get('CC','cc')
   p=subprocess.run([compiler,'-std=c11','-Wall','-Wextra','-Werror','-O1',
                     str(source),'-o',str(exe)],text=True,capture_output=True)
   self.assertEqual(p.returncode,0,p.stderr)
   p=subprocess.run([str(exe)],text=True,capture_output=True)
   self.assertEqual(p.returncode,0,p.stderr)

if __name__=='__main__':unittest.main()
