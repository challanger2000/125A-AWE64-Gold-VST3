#!/usr/bin/env python3
"""P117: observe ORIGINAL Pentium P54C selected timing/dependency words.
Operates ONLY on a disposable original-86Box copy already instrumented by
P107/P108/P110/P111/P113, without touching original guest, DSP or product.
Compiler-time decode events are NOT proof of actual executed x86 opcodes.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import sys

from p110_instrument_original_p54c_compile_timing import blob
from p113_tag_original_p110_opcode_agi_with_p111_compile_epoch import P113_MARKER
from p111_join_original_p54c_jit_compile_execute import P111_MARKER

PINNED_TIMING='aa78ee1c96076eef581ce4f1b5a86ae6c47d82a4'
MARKER='P117_ORIGINAL_P54C_SELECTED_OPCODE_JIT_EPOCH_V1'
TIMING_INSERT='''static uint8_t last_prefix;
static int     prefixes;
'''
ENTRY='''    int       agi_stall = 0;

    switch (last_prefix) {'''
BEFORE_AGI='''    if (u_pipe_full) {'''
JIT_DECL_INSERT='''static int codegen_block_ins;'''
JIT_HOOK='''    p113_capture_original_agi_opcode(block,old_pc,opcode);
    p110_original_p54c_translator_opcode_after();'''

TIMING_OBSERVER=r'''
/* P117_ORIGINAL_P54C_SELECTED_OPCODE_JIT_EPOCH_V1: original P54C
 * selected timing/dependency table values at compiler time.
 * Standalone source-level recorder; never changes pipeline/CPU/chip state.
 */
typedef struct {
    uint8_t input_opcode;
    uint8_t original_prefix;
    uint8_t selected_opcode;
    uint32_t fetchdat;
    uint32_t op_32;
    uint64_t timing_word;
    uint64_t dependency_word;
} p117_decoded_word;
static p117_decoded_word p117_last_selected;
static int p117_selected_pending;
static void
p117_p54c_record_selected(uint8_t input_opcode,uint8_t prefix,
                           uint32_t fetchdat,uint32_t op_32,
                           uint8_t selected_opcode,uint64_t timing,
                           uint64_t dependency)
{
    p117_last_selected.input_opcode=input_opcode;
    p117_last_selected.original_prefix=prefix;
    p117_last_selected.fetchdat=fetchdat;
    p117_last_selected.op_32=op_32;
    p117_last_selected.selected_opcode=selected_opcode;
    p117_last_selected.timing_word=timing;
    p117_last_selected.dependency_word=dependency;
    p117_selected_pending=1;
}
int
p117_p54c_consume_selected(p117_decoded_word *out)
{
    if (!out || !p117_selected_pending)return 0;
    *out=p117_last_selected;
    p117_selected_pending=0;
    return 1;
}
'''

JIT_OBSERVER=r'''
/* P117_ORIGINAL_P54C_SELECTED_OPCODE_JIT_EPOCH_V1:
 * Strict optional original P54C COMPILATION provenance; the original
 * P111 R/D records later confirm whether this generation actually ran,
 * but NOT whether an individual translated instruction executed.
 */
typedef struct {
    uint8_t input_opcode,original_prefix,selected_opcode;
    uint32_t fetchdat,op_32;
    uint64_t timing_word,dependency_word;
} p117_decoded_word;
extern int p117_p54c_consume_selected(p117_decoded_word *);
static FILE *p117_selected_file;
static int p117_selected_attempted;
static int p117_selected_enabled;
static uint64_t p117_selected_seq;
static uint32_t p117_selected_base;
static void p117_selected_close(void)
{
    if (p117_selected_file) {
        fflush(p117_selected_file);
        fclose(p117_selected_file);
        p117_selected_file=NULL;
    }
}
static void p117_selected_failed(void)
{
    if (!p117_selected_enabled)return;
    fputs("# P117_MISSING_ORIGINAL_P54C_EPOCH_OR_OPCODE_NO_GO\n",
          p117_selected_file);
    p117_selected_close();
    p117_selected_enabled=0;
}
static void p117_selected_init(void)
{
    char *tail=NULL;
    unsigned long long v;
    const char *path=getenv("AWE64_P117_ORIGINAL_SELECTED_TRACE");
    const char *base=getenv("AWE64_P117_LCOD_BASE");
    const char *sel=getenv("AWE64_P117_LCOD_SELECTOR");
    if (p117_selected_attempted)return;
    p117_selected_attempted=1;
    if (!path || !*path || !base || !*base || !sel || !*sel)return;
    v=strtoull(base,&tail,0);
    if (!tail || *tail || v>0xffff0000ULL)return;
    p117_selected_base=(uint32_t)v;
    v=strtoull(sel,&tail,0);
    if (!tail || *tail || v!=0x28u)return;
    p117_selected_file=fopen(path,"wb");
    if (!p117_selected_file)return;
    p117_selected_enabled=1;
    fputs("# P117_ORIGINAL_P54C_SELECTED_OPCODE_JIT_EPOCH_V1 "
          "P113_ORDERED_COMPILER_NOT_EXECUTION\n",p117_selected_file);
    atexit(p117_selected_close);
}
static void p117_capture_original_selected(codeblock_t *block,
                                             uint32_t pc,uint8_t opcode,
                                             uint32_t fetchdat,int op_32)
{
    p117_decoded_word word;
    uint32_t off,slot;
    uint64_t epoch;
    if (!p117_selected_attempted)p117_selected_init();
    if (!p117_selected_enabled)return;
    if (CS!=0x28u || pc<p117_selected_base)return;
    off=pc-p117_selected_base;
    if (off<0x4023u || off>0x4125u)return;
    if (!p117_p54c_consume_selected(&word) ||
        word.input_opcode!=opcode || word.fetchdat!=fetchdat ||
        word.op_32!=(uint32_t)op_32) {
        p117_selected_failed();
        return;
    }
    if ((off==0x4027u && opcode!=0x75u) ||
        (off==0x40cbu && opcode!=0x77u) ||
        (off==0x40e4u && opcode!=0x74u) ||
        (off==0x40f1u && opcode!=0x73u) ||
        (off==0x4107u && opcode!=0x72u)) {
        p117_selected_failed();
        return;
    }
    slot=(uint32_t)block->pnt;
    if (!p111_join_enabled || slot>=BLOCK_SIZE ||
        !p111_epoch_by_slot[slot] ||
        block->pc!=p111_pc_by_slot[slot] ||
        block->phys!=p111_phys_by_slot[slot]) {
        p117_selected_failed();
        return;
    }
    epoch=p111_epoch_by_slot[slot];
    if (++p117_selected_seq>500000ULL) {
        p117_selected_failed();
        return;
    }
    fprintf(p117_selected_file,
            "D %llu %llu %08X %02X %02X %08X %08X %02X %016llX %016llX\n",
            (unsigned long long)p117_selected_seq,
            (unsigned long long)epoch,(unsigned)pc,
            (unsigned)word.input_opcode,(unsigned)word.original_prefix,
            (unsigned)word.fetchdat,(unsigned)word.op_32,
            (unsigned)word.selected_opcode,
            (unsigned long long)word.timing_word,
            (unsigned long long)word.dependency_word);
}
'''

def once(src:str,original:str,replacement:str)->str:
    if src.count(original)!=1:
        raise ValueError('P117 source anchor absent/duplicated '+repr(original[:80]))
    return src.replace(original,replacement,1)

def patch(original:bytes,timing_copy:bytes,codegen_copy:bytes)->tuple[bytes,bytes]:
    if blob(original)!=PINNED_TIMING:
        raise ValueError('P117 original 86Box Pentium SHA mismatch')
    pristine=original.replace(b'\r\n',b'\n').decode('utf8')
    timing=timing_copy.replace(b'\r\n',b'\n').decode('utf8')
    codegen=codegen_copy.replace(b'\r\n',b'\n').decode('utf8')
    if MARKER in timing or MARKER in codegen:
        raise ValueError('P117 already instrumented')
    if (TIMING_INSERT not in pristine or ENTRY not in pristine or
        BEFORE_AGI not in pristine):
        raise ValueError('P117 original Pentium selection anchors changed')
    if ('P110_ORIGINAL_P54C_TRANSLATION_TIMING_V1' not in timing or
        P111_MARKER not in codegen or P113_MARKER not in codegen or
        'p113_capture_original_agi_opcode' not in codegen):
        raise ValueError('P117 requires complete P110/P111/P113 probes')
    timing=once(timing,TIMING_INSERT,TIMING_INSERT+TIMING_OBSERVER)
    timing=once(timing,ENTRY,
        '''    int       agi_stall = 0;
    const uint8_t p117_original_opcode = opcode;
    const uint8_t p117_original_prefix = last_prefix;

    switch (last_prefix) {''')
    # Must instrument ONLY after the original table-selection switch.
    start=timing.find('codegen_timing_pentium_opcode(uint8_t opcode')
    if start<0:raise ValueError('P117 missing original opcode entry point')
    at=timing.find(BEFORE_AGI,start)
    if at<0:raise ValueError('P117 missing original post-selection anchor')
    selected='''    p117_p54c_record_selected(p117_original_opcode,
                              p117_original_prefix,fetchdat,(uint32_t)op_32,
                              opcode,timings[opcode],deps[opcode]);
'''
    timing=timing[:at]+selected+timing[at:]
    codegen=once(codegen,JIT_DECL_INSERT,JIT_OBSERVER+JIT_DECL_INSERT)
    codegen=once(codegen,JIT_HOOK,
        '''    p113_capture_original_agi_opcode(block,old_pc,opcode);
    p117_capture_original_selected(block,old_pc,opcode,fetchdat,op_32);
    p110_original_p54c_translator_opcode_after();''')
    return timing.encode('utf8'),codegen.encode('utf8')

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--original-pentium',type=Path,required=True)
    a.add_argument('--copy-pentium',type=Path,required=True)
    a.add_argument('--copy-old-codegen',type=Path,required=True)
    a.add_argument('--verify-only',action='store_true')
    x=a.parse_args()
    try:
        if x.original_pentium.resolve()==x.copy_pentium.resolve():
            raise ValueError('refuse original source mutation')
        out_t,out_c=patch(x.original_pentium.read_bytes(),
                          x.copy_pentium.read_bytes(),
                          x.copy_old_codegen.read_bytes())
        if not x.verify_only:
            x.copy_pentium.write_bytes(out_t)
            x.copy_old_codegen.write_bytes(out_c)
        print('P117_SHA_PINNED_ORIGINAL_P54C_SELECTED_OPCODE_EPOCH_SOURCE_PASS '
              'original_binary_uploaded=NO win95_executed=NO product_modified=NO')
    except (OSError,ValueError,UnicodeError) as err:
        print('P117_SOURCE_NO_GO '+str(err),file=sys.stderr)
        return 2
    return 0

if __name__=='__main__':raise SystemExit(main())
