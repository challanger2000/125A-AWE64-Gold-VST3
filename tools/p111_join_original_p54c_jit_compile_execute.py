#!/usr/bin/env python3
"""P111 – private original OLD-x64 86Box JIT compilation/execution join probe.

Adds opt-in real COMPILED (C/F) vs EXECUTED (R/D) old Dynarec block IDs.
Only to a disposable SHA-pinned original turican0 86Box clone already patched
with P107 x86 I, P108 Jcc B, P110 original Pentium TRANSLATOR.

A compiled block may never execute: never conflate original P110 AGI
translation side-effects with runtime P54C TSC/PCM. Each *successful*
compiled block generation is assigned a monotonically increasing token
linked to actual runtime block->pnt, block->pc, block->phys; reused slots
get a new generation, never inherit old original C/F measurements.

No Win95 ROM/VxD, Creative DSP, SF2, original PCM or product VST3 changed.
"""
from __future__ import annotations
from pathlib import Path
import argparse
import hashlib
import sys

PINNED_386_DYNAREC_BLOB="e9765a423b5d675337aab33fd060204e62dc20d7"
P110_MARKER="P110_ORIGINAL_P54C_TRANSLATION_TIMING_V1"
P107_MARKER="P107_OLD_DYNAREC_INTERPRETER_FALLBACK_V2"
P108_MARKER="P108_DIRECT_EXECUTED_X86_JCC_CF_ZF_V1"
P111_MARKER="P111_ORIGINAL_P54C_COMPILE_TO_EXECUTE_JIT_JOIN_V1"
JIT_GLOBAL_ANCHOR="int        block_pos;\n"
COMPILE_START_ANCHOR="""    p110_original_p54c_translator_block_begin(cpu_state.pc, (uint16_t)CS);
    codegen_timing_block_start();"""
COMPILE_END_ANCHOR="""    codegen_timing_block_end();
    codegen_accumulate(ACCREG_cycles, -codegen_block_cycles);"""
INTERP_RUN_ANCHOR="""        inrecomp = 1;
        code();
#    ifdef USE_ACYCS"""
INTERP_DECL_ANCHOR="#include \"386_common.h\""

JIT_CODE= r'''
/* P111_ORIGINAL_P54C_COMPILE_TO_EXECUTE_JIT_JOIN_V1 – trace private only.
 * Each block-generation and runtime-run have the same explicit epoch ID.
 * Unlike P110 translating x86 and P107 observing executed x86, this
 * connects only the original JIT-block generation and actual call code().
 * No arithmetic in the guest CPU clock and no chip/DSP mutation.
 */
static FILE *p111_join_file;
static int p111_join_attempted;
static int p111_join_enabled;
static uint32_t p111_join_lcod_base;
static uint64_t p111_join_seq;
static uint64_t p111_compile_epoch;
static uint64_t p111_epoch_by_slot[BLOCK_SIZE];
static uint32_t p111_pc_by_slot[BLOCK_SIZE];
static uint32_t p111_phys_by_slot[BLOCK_SIZE];
static uint8_t p111_compiled_by_slot[BLOCK_SIZE];

static void
p111_join_close(void)
{
    if (p111_join_file) {
        fflush(p111_join_file);
        fclose(p111_join_file);
        p111_join_file = NULL;
    }
}
static void
p111_join_init(void)
{
    const char *path,*base,*selector;
    char *tail=NULL;
    unsigned long long number;
    if (p111_join_attempted)return;
    p111_join_attempted=1;
    path=getenv("AWE64_P111_JIT_BLOCK_JOIN_TRACE");
    base=getenv("AWE64_P111_LCOD_BASE");
    selector=getenv("AWE64_P111_LCOD_SELECTOR");
    if (!path || !*path || !base || !*base || !selector || !*selector)return;
    number=strtoull(base,&tail,0);
    if (!tail || *tail || number>0xffff0000ULL)return;
    p111_join_lcod_base=(uint32_t)number;
    number=strtoull(selector,&tail,0);
    if (!tail || *tail || number!=0x28u)return;
    p111_join_file=fopen(path,"wb");
    if (!p111_join_file)return;
    p111_join_enabled=1;
    fputs("# P111_ORIGINAL_P54C_COMPILE_TO_EXECUTE_JIT_JOIN_V1 "
          "COMPILE_NOT_EXECUTED RUNTIME_BLOCK_NOT_INSTRUCTION_TSC\n",
          p111_join_file);
    atexit(p111_join_close);
}
static int
p111_join_range(uint32_t pc)
{
    return pc>=p111_join_lcod_base+0x3f00u &&
           pc<=p111_join_lcod_base+0x4200u;
}
static int
p111_join_emit_guard(void)
{
    if (!p111_join_enabled || !p111_join_file)return 0;
    if (++p111_join_seq>1000000ULL) {
        fputs("# P111_EVENT_CAP_REACHED_NO_COMPLETE_CLAIM\n",p111_join_file);
        p111_join_close();
        p111_join_enabled=0;
        return 0;
    }
    return 1;
}
static void
p111_join_flush(void)
{
    if (p111_join_file && ((p111_join_seq & 2047ULL)==0ULL))
        fflush(p111_join_file);
}
/* Compiler stage: a SLOT can be reused after invalidation/recompile.
 * The epoch is not the slot and must never be inferred from guest PC.
 */
static void
p111_jit_compilation_start(codeblock_t *block)
{
    uint32_t slot,pc,phys;
    if (!p111_join_attempted)p111_join_init();
    if (!p111_join_enabled)return;
    slot=(uint32_t)block->pnt;
    if (slot>=BLOCK_SIZE)return;
    p111_compiled_by_slot[slot]=0u;
    p111_epoch_by_slot[slot]=0u;
    pc=block->pc;
    phys=block->phys;
    if (CS!=0x28u || !p111_join_range(pc))return;
    p111_pc_by_slot[slot]=pc;
    p111_phys_by_slot[slot]=phys;
    p111_epoch_by_slot[slot]=++p111_compile_epoch;
    if (!p111_join_emit_guard())return;
    fprintf(p111_join_file,"C %llu %llu %u %08X %08X\n",
            (unsigned long long)p111_join_seq,
            (unsigned long long)p111_epoch_by_slot[slot],
            (unsigned)slot,(unsigned)pc,(unsigned)phys);
    p111_join_flush();
}
static void
p111_jit_compilation_finished(codeblock_t *block)
{
    uint32_t slot=(uint32_t)block->pnt;
    if (!p111_join_enabled || slot>=BLOCK_SIZE)return;
    if (!p111_epoch_by_slot[slot] ||
        block->pc!=p111_pc_by_slot[slot] ||
        block->phys!=p111_phys_by_slot[slot])return;
    if (!p111_join_emit_guard())return;
    p111_compiled_by_slot[slot]=1u;
    fprintf(p111_join_file,"F %llu %llu %u %08X %08X %d\n",
            (unsigned long long)p111_join_seq,
            (unsigned long long)p111_epoch_by_slot[slot],
            (unsigned)slot,(unsigned)block->pc,(unsigned)block->phys,
            codegen_block_cycles);
    p111_join_flush();
}
/* Runtime stage: the actual ORIGINAL old Dynarec code() call.
 * CPU TSC here is a BLOCK boundary; never an instruction-by-instruction TSC.
 */
void
p111_old_jit_runtime_boundary(uint32_t slot,uint32_t pc,uint32_t phys,
                              uint64_t original_guest_tsc,int on_exit)
{
    if (!p111_join_enabled || slot>=BLOCK_SIZE ||
        !p111_compiled_by_slot[slot] || !p111_epoch_by_slot[slot] ||
        pc!=p111_pc_by_slot[slot] || phys!=p111_phys_by_slot[slot])return;
    if (!p111_join_emit_guard())return;
    fprintf(p111_join_file,"%c %llu %llu %u %08X %08X %llu\n",
            on_exit ? 'D':'R',(unsigned long long)p111_join_seq,
            (unsigned long long)p111_epoch_by_slot[slot],
            (unsigned)slot,(unsigned)pc,(unsigned)phys,
            (unsigned long long)original_guest_tsc);
    p111_join_flush();
}
'''

def blob(raw:bytes)->str:
    raw=raw.replace(b"\r\n",b"\n")
    return hashlib.sha1(b"blob "+str(len(raw)).encode()+b"\0"+raw).hexdigest()

def once(s:str,a:str,b:str,label:str)->str:
    if s.count(a)!=1:raise ValueError("P111 original anchor absent/duplicate: "+label)
    return s.replace(a,b,1)

def patch_pair(pristine_dynarec:bytes,
               already_p107_interpreter_dynarec:bytes,
               already_p110_codegen:bytes,
               original_sha_check:bool=True)->tuple[bytes,bytes]:
    if original_sha_check and blob(pristine_dynarec)!=PINNED_386_DYNAREC_BLOB:
        raise ValueError("P111 ORIGINAL 386 dynarec source SHA pin mismatch")
    cpu=already_p107_interpreter_dynarec.replace(b"\r\n",b"\n").decode("utf8")
    code=already_p110_codegen.replace(b"\r\n",b"\n").decode("utf8")
    # P107's interpreter marker lives in the patched codegen supplement,
    # not in 386_dynarec.c. Check the real injected guest dispatch symbol.
    if "p107_old_dynarec_interpreter_exec" not in cpu:
        raise ValueError("P111 requires P107 interpreted guest-original source")
    if P107_MARKER not in code:
        raise ValueError("P111 requires P107 original executed-x86 codegen")
    for marker in (P110_MARKER,P108_MARKER):
        if marker not in code:raise ValueError("P111 requires P108/P110 old x64 JIT source")
    if P111_MARKER in cpu or P111_MARKER in code:
        raise ValueError("P111 already present in old 86Box")
    code=once(code,JIT_GLOBAL_ANCHOR,
              JIT_GLOBAL_ANCHOR+JIT_CODE,
              "original global original P54C JIT block section")
    code=once(code,COMPILE_START_ANCHOR,
'''    p111_jit_compilation_start(block);
'''+COMPILE_START_ANCHOR,
              "exact original compile-start timing gate")
    code=once(code,COMPILE_END_ANCHOR,
'''    codegen_timing_block_end();
    p111_jit_compilation_finished(block);
    codegen_accumulate(ACCREG_cycles, -codegen_block_cycles);''',
              "exact original compile-end booked timing gate")
    cpu=once(cpu,INTERP_DECL_ANCHOR,
INTERP_DECL_ANCHOR+'''
#if defined(USE_DYNAREC) && !defined(USE_NEW_DYNAREC) && (defined(__amd64__) || defined(_M_X64))
extern void p111_old_jit_runtime_boundary(uint32_t,uint32_t,uint32_t,uint64_t,int);
#endif
''', "original dynarec header")
    cpu=once(cpu,INTERP_RUN_ANCHOR,
'''        inrecomp = 1;
#if defined(USE_DYNAREC) && !defined(USE_NEW_DYNAREC) && (defined(__amd64__) || defined(_M_X64))
        p111_old_jit_runtime_boundary((uint32_t)block->pnt,block->pc,
                                      block->phys,tsc,0);
#endif
        code();
#if defined(USE_DYNAREC) && !defined(USE_NEW_DYNAREC) && (defined(__amd64__) || defined(_M_X64))
        p111_old_jit_runtime_boundary((uint32_t)block->pnt,block->pc,
                                      block->phys,tsc,1);
#endif
#    ifdef USE_ACYCS''', "exact original compiled block code() execution")
    return cpu.encode(),code.encode()

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--original-dynarec",type=Path,required=True)
    ap.add_argument("--copy-dynarec",type=Path,required=True)
    ap.add_argument("--copy-old-codegen",type=Path,required=True)
    ap.add_argument("--verify-only",action="store_true")
    a=ap.parse_args()
    try:
        pristine=a.original_dynarec.read_bytes()
        if blob(pristine)!=PINNED_386_DYNAREC_BLOB:
            raise ValueError("original P111 386 source SHA pin mismatch")
        if a.original_dynarec.resolve()==a.copy_dynarec.resolve():
            raise ValueError("P111 refuse original source mutation")
        cpu,code=patch_pair(pristine,a.copy_dynarec.read_bytes(),
                            a.copy_old_codegen.read_bytes())
        if not a.verify_only:
            a.copy_dynarec.write_bytes(cpu)
            a.copy_old_codegen.write_bytes(code)
        print("P111_ORIGINAL_P54C_JIT_COMPILE_EXECUTE_JOIN_SOURCE_PASS "+
              "original_guest_runtime=NOT_RUN "
              "original_dsp_unchanged=TRUE "
              "compile_only_never_misreported_as_execution=TRUE")
        return 0
    except (OSError,ValueError,UnicodeError) as e:
        print("P111_SOURCE_PIN_NO_GO "+str(e),file=sys.stderr)
        return 2
if __name__=="__main__":
    sys.exit(main())
