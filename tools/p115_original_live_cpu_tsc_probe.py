#!/usr/bin/env python3
"""P115: opt-in original 86Box read-only CPU snapshot at existing P003 physical ports.
Patch only disposable old-x64 Dynarec and P003-instrumented io.c copies.
Original pinned driver, ROM, sample, guest disk, DSP and product stay untouched.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import sys

ORIGINAL_DYNAREC_BLOB="e9765a423b5d675337aab33fd060204e62dc20d7"
P003_IO_MARKER="P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK"
P115_MARKER="P115_ORIGINAL_DYNAREC_LIVE_CPU_TSC_V1"
ANCHOR_CPU='''#ifdef USE_DYNAREC
int32_t         cycles_main = 0;
static int32_t  cycles_old  = 0;
static uint64_t tsc_old     = 0;

#    ifdef USE_ACYCS
int32_t acycs = 0;
#    endif
'''
ANCHOR_IO='''static void p003_cpu_io_note(char direction, uint16_t port, uint16_t value)
'''
END_IO='''}
/* Exact EMU8K byte-handler calling semantics'''

CPU_PROBE=r'''/* P115_ORIGINAL_DYNAREC_LIVE_CPU_TSC_V1.
 * Read ORIGINAL run-time state; NEVER call update_tsc(), timer_process(),
 * a translated instruction, or a hardware handler while observing it.
 * Return 0 for unavailable Dynarec state, -1 for inconsistent clock,
 * 1 for a valid, source-formula projected clock at an actual port callback.
 */
int
p115_original_dynarec_live_tsc(uint64_t *effective, uint64_t *raw,
    uint64_t *origin, int32_t *at_block, int32_t *remaining,
    int32_t *aux, int *jit)
{
    int64_t spent;
    uint64_t booked, extra;
    if (!effective || !raw || !origin || !at_block || !remaining || !aux || !jit)
        return -1;
    if (!cpu_use_dynarec) return 0;
    *raw=tsc;
    *origin=tsc_old;
    *at_block=cycles_old;
    *remaining=cycles;
    *jit=(inrecomp != 0);
    *aux=0;
    spent=(int64_t)cycles_old-(int64_t)cycles;
#    ifdef USE_ACYCS
    if (inrecomp) {
        *aux=acycs;
        spent+=(int64_t)acycs;
    }
#    endif
    if (tsc < tsc_old) return -1;
    booked=tsc-tsc_old;
    if (spent<=0 || (uint64_t)spent<=booked) {
        *effective=tsc;
        return 1;
    }
    extra=(uint64_t)spent-booked;
    if (UINT64_MAX-tsc<extra) return -1;
    *effective=tsc+extra;
    return 1;
}
'''

IO_PROBE=r'''/* P115_ORIGINAL_DYNAREC_LIVE_CPU_TSC_V1: only physical EMU8K
 * callbacks already observed by P003. Requires BOTH P003 and P115 opt-in.
 * No modification of EMU8K, no guest stack or driver bytes, no timers.
 * The P003 callback has incremented p003_cpu_io_seq before this call.
 */
#ifdef USE_DYNAREC
extern int p115_original_dynarec_live_tsc(uint64_t*,uint64_t*,uint64_t*,
                                         int32_t*,int32_t*,int32_t*,int*);
static FILE *p115_original_cpu_file;
static int p115_original_cpu_attempted;
static uint64_t p115_original_cpu_events;
static void p115_original_cpu_close(void)
{
    if (p115_original_cpu_file) {
        fflush(p115_original_cpu_file);
        fclose(p115_original_cpu_file);
        p115_original_cpu_file=NULL;
    }
}
static void p115_original_cpu_port(char direction, uint16_t port,
                                   unsigned long long p003_seq)
{
    uint64_t effective=0,raw=0,origin=0;
    int32_t at_block=0,remaining=0,aux=0;
    int jit=0,ok;
    if (!p003_cpu_io_file) return;
    if (!p115_original_cpu_attempted) {
        const char *path=getenv("AWE64_P115_LIVE_TSC_TRACE");
        p115_original_cpu_attempted=1;
        if (path && *path) {
            p115_original_cpu_file=fopen(path,"wb");
            if (p115_original_cpu_file) {
                fputs("# P115_ORIGINAL_DYNAREC_LIVE_CPU_TSC_V1 "
                      "P003_SEQUENCE_SAME_PHYSICAL_IO_READ_ONLY\n",
                      p115_original_cpu_file);
                atexit(p115_original_cpu_close);
            }
        }
    }
    if (!p115_original_cpu_file) return;
    if (++p115_original_cpu_events>1000000ULL) {
        fputs("# P115_EVENT_CAP_REACHED_NO_COMPLETE_CLAIM\n",p115_original_cpu_file);
        p115_original_cpu_close();
        return;
    }
    ok=p115_original_dynarec_live_tsc(&effective,&raw,&origin,
                                    &at_block,&remaining,&aux,&jit);
    if (ok!=1) {
        fprintf(p115_original_cpu_file,"# P115_CPU_SNAPSHOT_UNAVAILABLE_NO_GO %d\n",ok);
        p115_original_cpu_close();
        return;
    }
    fprintf(p115_original_cpu_file,
            "S %llu %c %04X %llu %llu %llu %d %d %d %d\n",
            p003_seq,direction,(unsigned)port,
            (unsigned long long)raw,(unsigned long long)effective,
            (unsigned long long)origin,(int)at_block,(int)remaining,
            (int)aux,jit);
}
#endif
'''

def git_blob(data:bytes)->str:
    clean=data.replace(b'\r\n',b'\n')
    return hashlib.sha1(b'blob '+str(len(clean)).encode()+b'\x00'+clean).hexdigest()

def one(source:str,anchor:str,replacement:str)->str:
    if source.count(anchor)!=1:
        raise ValueError('P115 missing/duplicate source anchor: '+repr(anchor[:80]))
    return source.replace(anchor,replacement,1)

def patch(dynarec_original:bytes,dynarec_copy:bytes,p003_io_copy:bytes)->tuple[bytes,bytes]:
    if git_blob(dynarec_original)!=ORIGINAL_DYNAREC_BLOB:
        raise ValueError('P115 SHA-pinned original old Dynarec source mismatch')
    dyn=dynarec_copy.replace(b'\r\n',b'\n').decode('utf-8')
    src=dynarec_original.replace(b'\r\n',b'\n').decode('utf-8')
    io=p003_io_copy.replace(b'\r\n',b'\n').decode('utf-8')
    if (P115_MARKER in dyn or P115_MARKER in io):
        raise ValueError('P115 already instrumented')
    if (ANCHOR_CPU not in src or ANCHOR_CPU not in dyn):
        raise ValueError('P115 original CPU snapshot anchor absent')
    if (P003_IO_MARKER not in io or ANCHOR_IO not in io):
        raise ValueError('P115 requires previously P003-instrumented physical IO')
    if 'p003_cpu_io_seq++' not in io:
        raise ValueError('P115 requires original P003 physical callback ordering')
    dyn=one(dyn,ANCHOR_CPU,ANCHOR_CPU+'\n'+CPU_PROBE)
    io=one(io,ANCHOR_IO,IO_PROBE+'\n'+ANCHOR_IO)
    io=one(io,END_IO,'''#ifdef USE_DYNAREC
    if (p003_cpu_io_file && p003_cpu_io_seq)
        p115_original_cpu_port(direction,port,p003_cpu_io_seq-1u);
#endif
'''+END_IO)
    return dyn.encode('utf-8'),io.encode('utf-8')

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--original-dynarec',type=Path,required=True)
    ap.add_argument('--copy-dynarec',type=Path,required=True)
    ap.add_argument('--copy-p003-io',type=Path,required=True)
    ap.add_argument('--verify-only',action='store_true')
    a=ap.parse_args()
    try:
        original=a.original_dynarec.read_bytes()
        if a.original_dynarec.resolve()==a.copy_dynarec.resolve():
            raise ValueError('P115 refuses to edit source-original Dynarec')
        patched_dyn,patched_io=patch(original,a.copy_dynarec.read_bytes(),a.copy_p003_io.read_bytes())
        if not a.verify_only:
            a.copy_dynarec.write_bytes(patched_dyn)
            a.copy_p003_io.write_bytes(patched_io)
        print('P115_SOURCE_ONLY_ORIGINAL_RUNTIME_CPU_SNAPSHOT_READY original_sha='+git_blob(original)+
              ' no_original_writes=TRUE no_dsp_writes=TRUE actual_win95_run=NOT_TESTED')
        return 0
    except (OSError,UnicodeError,ValueError) as ex:
        print('P115_SOURCE_NO_GO '+str(ex),file=sys.stderr)
        return 2
if __name__=='__main__':raise SystemExit(main())
