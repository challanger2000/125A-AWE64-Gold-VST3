#!/usr/bin/env python3
"""P113: SHA-pinned original 86Box P110-OPCODE AGI => P111 JIT EPOCH tagging.

P110 original Pentium timing check_agi() is still the only AGI source.
P111 original old Dynarec compile_epoch is still the only block identity.
The new PRIVATE (disabled-by-default) E event is emitted *after* original
codegen_timing_opcode and *before* existing P110 opcode_after clears active
AGI counters. It captures actual original P110 AGI counts with the exact
P111 generation of the compiling block, NOT instruction CPU execution time.

P113 is research only: no EMU8000 DSP/Win95/ROM/SF2/VST3 mutation.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import sys

from p110_instrument_original_p54c_compile_timing import (
    blob as git_blob,P110_TIMING
)
from p111_join_original_p54c_jit_compile_execute import JIT_CODE,P111_MARKER

P113_MARKER="P113_ORIGINAL_P54C_AGI_OPCODE_JIT_EPOCH_V1"
P110_GETTER_INSERT="static void\np110_translator_file_close(void)"
CODEGEN_INSERT="static int codegen_block_ins;"
JIT_OP_AFTER="    p110_original_p54c_translator_opcode_after();"
P113_TIMING_GETTER=r'''
/* P113_ORIGINAL_P54C_AGI_OPCODE_JIT_EPOCH_V1: only observe original
 * P110 check_agi return counters, never calculate replacement AGI.
 */
int
p113_original_p54c_get_last_opcode_agi(uint32_t *calls,uint32_t *positive)
{
    if (!p110_translator_active || !p110_translator_file)return 0;
    *calls=p110_translator_agi_calls;
    *positive=p110_translator_agi_positive;
    return 1;
}
'''
P113_JIT=r'''
/* P113_ORIGINAL_P54C_AGI_OPCODE_JIT_EPOCH_V1 – original JIT
 * compilation provenance for P110 AGI. Disabled without explicit
 * private path+original LCOD+selector, and without P110/P111 opt-in.
 */
extern int p113_original_p54c_get_last_opcode_agi(uint32_t *,uint32_t *);
static FILE *p113_original_agi_epoch_file;
static int p113_agi_epoch_attempted;
static int p113_agi_epoch_enabled;
static uint32_t p113_agi_epoch_base;
static uint64_t p113_agi_epoch_sequence;
static void
p113_agi_epoch_close(void)
{
    if (p113_original_agi_epoch_file) {
        fflush(p113_original_agi_epoch_file);
        fclose(p113_original_agi_epoch_file);
        p113_original_agi_epoch_file=NULL;
    }
}
static void
p113_agi_epoch_init(void)
{
    char *tail=NULL;
    const char *path,*base,*selector;
    unsigned long long value;
    if (p113_agi_epoch_attempted)return;
    p113_agi_epoch_attempted=1;
    path=getenv("AWE64_P113_ORIGINAL_AGI_EPOCH_TRACE");
    base=getenv("AWE64_P113_LCOD_BASE");
    selector=getenv("AWE64_P113_LCOD_SELECTOR");
    if (!path || !*path || !base || !*base || !selector || !*selector)
        return;
    value=strtoull(base,&tail,0);
    if (!tail || *tail || value>0xffff0000ULL)return;
    p113_agi_epoch_base=(uint32_t)value;
    value=strtoull(selector,&tail,0);
    if (!tail || *tail || value!=0x28u)return;
    p113_original_agi_epoch_file=fopen(path,"wb");
    if (!p113_original_agi_epoch_file)return;
    p113_agi_epoch_enabled=1;
    fputs("# P113_ORIGINAL_P54C_AGI_OPCODE_JIT_EPOCH_V1 "
          "COMPILED_OPCODE_NOT_EXECUTED_X86 "
          "ONLY_ORIGINAL_P110_CHECK_AGI_COUNTS\n",
          p113_original_agi_epoch_file);
    atexit(p113_agi_epoch_close);
}
static void
p113_agi_epoch_no_go(void)
{
    if (!p113_agi_epoch_enabled)return;
    fputs("# P113_EPOCH_ORIGINAL_PROVENANCE_MISSING_NO_GO\n",
          p113_original_agi_epoch_file);
    p113_agi_epoch_close();
    p113_agi_epoch_enabled=0;
}
static void
p113_capture_original_agi_opcode(codeblock_t *block,uint32_t pc,uint8_t opcode)
{
    uint32_t calls=0u,positive=0u,slot;
    uint64_t epoch;
    if (!p113_agi_epoch_attempted)p113_agi_epoch_init();
    if (!p113_agi_epoch_enabled)return;
    /* The original P110 source decides whether this exact old Dynarec
     * translated opcode belongs to the actually observed LCOD range. */
    if (!p113_original_p54c_get_last_opcode_agi(&calls,&positive))return;
    if (CS!=0x28u || pc<p113_agi_epoch_base+0x4023u ||
        pc>p113_agi_epoch_base+0x4125u)return;
    slot=(uint32_t)block->pnt;
    if (!p111_join_enabled || slot>=BLOCK_SIZE ||
        !p111_epoch_by_slot[slot] || !p111_pc_by_slot[slot] ||
        block->pc!=p111_pc_by_slot[slot] ||
        block->phys!=p111_phys_by_slot[slot]) {
        p113_agi_epoch_no_go();
        return;
    }
    epoch=p111_epoch_by_slot[slot];
    if (++p113_agi_epoch_sequence>500000ULL) {
        p113_agi_epoch_no_go();
        return;
    }
    fprintf(p113_original_agi_epoch_file,
            "E %llu %llu %08X %02X %u %u\n",
            (unsigned long long)p113_agi_epoch_sequence,
            (unsigned long long)epoch,(unsigned)pc,(unsigned)opcode,
            (unsigned)calls,(unsigned)positive);
    if ((p113_agi_epoch_sequence & 2047ULL)==0ULL)
        fflush(p113_original_agi_epoch_file);
}
'''

def once(src:str,old:str,new:str)->str:
    if src.count(old)!=1:
        raise ValueError("P113 original P110/P111 timing hook anchor absent or repeated: "+old[:65])
    return src.replace(old,new,1)

def patch_pair(original_p110_timing:bytes,original_p111_codegen:bytes):
    timing=original_p110_timing.replace(b"\r\n",b"\n").decode("utf8")
    code=original_p111_codegen.replace(b"\r\n",b"\n").decode("utf8")
    if "P110_ORIGINAL_P54C_TRANSLATION_TIMING_V1" not in timing or (
        "p110_translator_agi_positive" not in timing or
        "p110_original_p54c_translator_opcode_before" not in code or
        P111_MARKER not in code or
        "p111_jit_compilation_start(block)" not in code):
        raise ValueError("P113 requires complete original P110 and P111 instrumentation")
    if P113_MARKER in timing or P113_MARKER in code:
        raise ValueError("P113 original Pentium source already patched")
    timing=once(timing,P110_GETTER_INSERT,
                P113_TIMING_GETTER+P110_GETTER_INSERT)
    code=once(code,CODEGEN_INSERT,P113_JIT+CODEGEN_INSERT)
    code=once(code,JIT_OP_AFTER,
              "    p113_capture_original_agi_opcode(block,old_pc,opcode);\n"+JIT_OP_AFTER)
    return timing.encode(),code.encode()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--original-pentium-timing",type=Path,required=True)
    p.add_argument("--copy-pentium-timing",type=Path,required=True)
    p.add_argument("--copy-old-codegen",type=Path,required=True)
    p.add_argument("--verify-only",action="store_true")
    a=p.parse_args()
    try:
        original=a.original_pentium_timing.read_bytes()
        copy=a.copy_pentium_timing.read_bytes()
        if git_blob(original)!="aa78ee1c96076eef581ce4f1b5a86ae6c47d82a4":
            raise ValueError("P113 original 86Box Pentium source SHA mismatch")
        if a.original_pentium_timing.resolve()==a.copy_pentium_timing.resolve():
            raise ValueError("P113 refuse original 86Box source mutation")
        # P110 instrumented disposable source still has full original
        # P54C logic; original file provides SHA pinned provenance.
        timing,code=patch_pair(copy,a.copy_old_codegen.read_bytes())
        if not a.verify_only:
            a.copy_pentium_timing.write_bytes(timing)
            a.copy_old_codegen.write_bytes(code)
        print("P113_ORIGINAL_P110_AGI_TO_P111_JIT_EPOCH_SOURCE_PASS "
              "original_cpu_guest=NOT_RUN original_emu8000_dsp_unchanged=TRUE "
              "translated_opcode_not_executed_instruction=TRUE")
    except (ValueError,OSError,UnicodeError) as e:
        print("P113_SOURCE_NO_GO "+str(e),file=sys.stderr)
        return 2
    return 0
if __name__=="__main__":sys.exit(main())
