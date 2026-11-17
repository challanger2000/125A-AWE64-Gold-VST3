#!/usr/bin/env python3
"""P119 capture original old-x64 compiler-deferred cycle charges at runtime.

The ORIGINAL 86Box codegen_accumulate_x86-64.c holds the not-yet-emitted
guest _cycles delta in acc_regs[ACCREG_cycles].count. Extract it via a
read-only compile-time getter, bake the signed 32-bit value into the
FOURTH argument of the already existing P118/P107 guest-JIT callback.
At runtime record the original *memory* cycles and pending compiler
delta, with no generated flush/memory write or timer changes.
This is not proof of retired x86 opcode clocks or autonomous PCM.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import sys

from p107_instrument_old_dynarec import (
    PINNED_UPSTREAM_SOURCE_BLOB as ORIGINAL_JIT_SHA,git_blob_sha1)
from p118_bind_runtime_jit_to_p117_opcode import P118_MARKER

ORIGINAL_ACC_SHA='6e88487badb8abb5b73258b2bb2a6cfcd97d65c3'
P119_MARKER='P119_ORIGINAL_X64_JIT_PENDING_CYCLES_V1'
ACC_ANCHOR='''void
codegen_accumulate(int acc_reg, int delta)
'''
JIT_CALLBACK_ANCHOR='''        addlong(p118_token);
        call(block, (uintptr_t)p118_x86_executed_guest_pc);'''
CODEGEN_ANCHOR='''static int codegen_block_ins;'''

READONLY_GETTER=r'''
/* P119_ORIGINAL_X64_JIT_PENDING_CYCLES_V1.
 * Return the ORIGINAL compiler's deferred cycles memory delta.
 * NEVER flush, reset, apply or change acc_regs in this observer.
 */
int32_t
p119_original_compiler_pending_cycles(void)
{
    return acc_regs[ACCREG_cycles].count;
}

'''

RUNTIME_CODE=r'''
/* P119_ORIGINAL_X64_JIT_PENDING_CYCLES_V1.
 * Original guest CPU cycles are a memory value; the original 86Box
 * JIT may postpone committed cycle changes in the compiler-local
 * acc_regs[].count. Record BOTH at the instrumented runtime JIT site
 * and never conflate them with precise per-opcode completion timing.
 */
extern int32_t p119_original_compiler_pending_cycles(void);
extern int inrecomp;
static FILE *p119_pending_file;
static int p119_pending_attempted;
static int p119_pending_enabled;
static uint64_t p119_pending_record_count;
static void p119_pending_close(void)
{
    if (p119_pending_file) {
        fflush(p119_pending_file);
        fclose(p119_pending_file);
        p119_pending_file=NULL;
    }
}
static void p119_pending_no_go(void)
{
    if (!p119_pending_enabled)return;
    fputs("# P119_ORIGINAL_SOURCE_PROVENANCE_NO_GO\n",p119_pending_file);
    p119_pending_close();
    p119_pending_enabled=0;
}
static void p119_pending_init(void)
{
    const char *path;
    if (p119_pending_attempted)return;
    p119_pending_attempted=1;
    path=getenv("AWE64_P119_PENDING_JIT_CYCLES_TRACE");
    if (!path || !*path)return;
    p119_pending_file=fopen(path,"wb");
    if (!p119_pending_file)return;
    p119_pending_enabled=1;
    fputs("# P119_ORIGINAL_X64_JIT_PENDING_CYCLES_V1 "
          "P118_SITE_P107_SEQ_CYCLES_MEM_AND_DEFERRED_DELTA\n",
          p119_pending_file);
    atexit(p119_pending_close);
}
static void
p119_x86_executed_guest_site(uint32_t pc,uint32_t ebp,uint32_t site,
                              int32_t pending)
{
    int aux=0;
    /* Retain the ONE genuine P107 execution callback through P118. */
    p118_x86_executed_guest_pc(pc,ebp,site);
    if (!p119_pending_attempted)p119_pending_init();
    if (!p119_pending_enabled)return;
    if (!site || !p118_site_file || !p107_x86_trace_file ||
        !p111_join_file || !p117_selected_file || !p107_x86_trace_seq) {
        p119_pending_no_go();
        return;
    }
    if (++p119_pending_record_count>1000000ULL) {
        fputs("# P119_EVENT_CAP_REACHED_NO_COMPLETE_CLAIM\n",p119_pending_file);
        p119_pending_close();
        p119_pending_enabled=0;
        return;
    }
#ifdef USE_ACYCS
    aux=acycs;
#endif
    fprintf(p119_pending_file,
            "Y %llu %u %08X %llu %d %d %d %d\n",
            (unsigned long long)(p107_x86_trace_seq-1u),
            (unsigned)site,(unsigned)pc,(unsigned long long)tsc,
            (int)cycles,(int)pending,aux,(int)(inrecomp!=0));
    if ((p119_pending_record_count & 2047ULL)==0ULL)
        fflush(p119_pending_file);
}
'''

JIT_FOURTH=r'''        addlong(p118_token);
        /* P119 compiler-visible source-original accumulated cycle delta.
         * This constant is emitted into the existing ORIGINAL host JIT
         * call. It is NEVER applied to guest cycles by this probe.
         * Windows R9D is argument four; Linux SysV ECX is argument four.
         */
#if _WIN64
        addbyte(0x41); addbyte(0xb9); /* MOV R9D, imm32 */
#else
        addbyte(0xb9); /* MOV ECX, imm32 */
#endif
        addlong((uint32_t)p119_original_compiler_pending_cycles());
        call(block, (uintptr_t)p119_x86_executed_guest_site);'''

def git_blob(raw:bytes)->str:
    return git_blob_sha1(raw)

def once(source:str,anchor:str,replacement:str)->str:
    if source.count(anchor)!=1:
        raise ValueError('P119 original deferred-cycle/JIT source anchor absent/duplicated: '+repr(anchor[:80]))
    return source.replace(anchor,replacement,1)

def patch(original_acc:bytes,acc_copy:bytes,
          original_jit:bytes,jit_copy:bytes)->tuple[bytes,bytes]:
    if git_blob(original_acc)!=ORIGINAL_ACC_SHA or git_blob(original_jit)!=ORIGINAL_JIT_SHA:
        raise ValueError('P119 original old-x64 accumulate/JIT source pin mismatch')
    acc=acc_copy.replace(b'\r\n',b'\n').decode('utf8')
    jit=jit_copy.replace(b'\r\n',b'\n').decode('utf8')
    if P119_MARKER in acc or P119_MARKER in jit:
        raise ValueError('P119 original source already patched')
    if ACC_ANCHOR not in original_acc.replace(b'\r\n',b'\n').decode('utf8'):
        raise ValueError('P119 original accumulate source signature changed')
    if P118_MARKER not in jit or JIT_CALLBACK_ANCHOR not in jit:
        raise ValueError('P119 requires original P118 JIT site probe')
    acc=once(acc,ACC_ANCHOR,READONLY_GETTER+ACC_ANCHOR)
    jit=once(jit,CODEGEN_ANCHOR,RUNTIME_CODE+CODEGEN_ANCHOR)
    jit=once(jit,JIT_CALLBACK_ANCHOR,JIT_FOURTH)
    # Must not move original codegen_accumulate_flush() or its queued charges.
    if '    codegen_accumulate(ACCREG_cycles, -codegen_block_cycles);' not in jit:
        raise ValueError('P119 original JIT source charge site missing')
    if jit.count('p119_original_compiler_pending_cycles()')!=1:
        raise ValueError('P119 compiler pending charge not embedded exactly once')
    return acc.encode('utf8'),jit.encode('utf8')

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--pristine-acc',type=Path,required=True)
    ap.add_argument('--copy-acc',type=Path,required=True)
    ap.add_argument('--pristine-old-codegen',type=Path,required=True)
    ap.add_argument('--copy-old-codegen',type=Path,required=True)
    ap.add_argument('--verify-only',action='store_true')
    a=ap.parse_args()
    try:
        if a.pristine_acc.resolve()==a.copy_acc.resolve() or (
            a.pristine_old_codegen.resolve()==a.copy_old_codegen.resolve()):
            raise ValueError('P119 refuses to alter original 86Box source')
        acc,code=patch(a.pristine_acc.read_bytes(),a.copy_acc.read_bytes(),
                       a.pristine_old_codegen.read_bytes(),
                       a.copy_old_codegen.read_bytes())
        if not a.verify_only:
            a.copy_acc.write_bytes(acc)
            a.copy_old_codegen.write_bytes(code)
        print('P119_ORIGINAL_DEFERRED_CYCLE_GETTER_JIT_IMMEDIATE_PATCH_PASS '
              'original_86box_source=PINNED no_cpu_timing_writes=TRUE '
              'no_guest_executed=TRUE')
    except (OSError,ValueError,UnicodeError) as exc:
        print('P119_SOURCE_NO_GO '+str(exc),file=sys.stderr)
        return 2
    return 0

if __name__=='__main__':raise SystemExit(main())
