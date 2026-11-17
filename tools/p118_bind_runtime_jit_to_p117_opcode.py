#!/usr/bin/env python3
"""P118: bind actual old-x64 JIT-site ENTRY to P117 decoded compile-site ID.

P117 captures the ORIGINAL P54C table words + P111 compilation epoch.
P107 emits a host callback on entry to individual x86 guest JIT sites.
This source patch embeds the exact compiler-side P117 token as a JIT
immediate argument in the EXISTING P107 callback (no second host call).
A reused JIT block slot or repeated guest PC never substitutes another
compilation's P54C words.

This proves JIT entry through the instrumented callback, not that the
guest instruction retired, its executed cycles or autonomous DRY PCM.
Works only on disposable ORIGINAL SHA-pinned old-86Box source with
P107/P108/P110/P111/P113/P117 already patched; no chip modifications.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import sys

from p107_instrument_old_dynarec import (
    INSERT_BEFORE as P107_INSERT, MARKER as P107_MARKER,
    PINNED_UPSTREAM_SOURCE_BLOB as ORIGINAL_CODEGEN_SHA,git_blob_sha1)
from p111_join_original_p54c_jit_compile_execute import P111_MARKER
from p117_tag_original_p54c_selected_opcode_epoch import MARKER as P117_MARKER

P118_MARKER="P118_ORIGINAL_JIT_EXECUTED_SITE_P117_TOKEN_V1"
JIT_GLOBAL_ANCHOR="static uint64_t p117_selected_seq;"
RESET_ANCHOR="    p117_decoded_word word;\n    uint32_t off,slot;"
SUCCESS_ANCHOR="    fprintf(p117_selected_file,"
P107_CALLBACK_ANCHOR="""        load_param_1_32(block, old_pc);
        load_param_2_reg_32(LOAD_REG_L(REG_EBP));
        call(block, (uintptr_t)p107_x86_executed_guest_pc);"""
P117_CAPTURE_ANCHOR="    p117_capture_original_selected(block,old_pc,opcode,fetchdat,op_32);"

JIT_CODE=r'''
/* P118_ORIGINAL_JIT_EXECUTED_SITE_P117_TOKEN_V1.
 * The site token is a compile-unique ID, not an inferred guest PC key.
 * P117/P111 source fields are looked up only while THIS instruction
 * is being translated, then baked into actual host JIT machine code.
 */
static uint32_t p118_last_p117_pc;
static uint32_t p118_last_p117_site;
static uint64_t p118_last_p117_epoch;
static FILE *p118_site_file;
static int p118_site_attempted;
static int p118_site_enabled;
static uint64_t p118_site_record_count;

static void p118_site_close(void)
{
    if (p118_site_file) {
        fflush(p118_site_file);
        fclose(p118_site_file);
        p118_site_file=NULL;
    }
}
static void p118_site_no_go(void)
{
    if (!p118_site_enabled)return;
    fputs("# P118_SOURCE_PROVENANCE_MISSING_NO_GO\n",p118_site_file);
    p118_site_close();
    p118_site_enabled=0;
}
static void p118_site_init(void)
{
    const char *path;
    if (p118_site_attempted)return;
    p118_site_attempted=1;
    path=getenv("AWE64_P118_EXECUTED_SITE_TRACE");
    if (!path || !*path)return;
    p118_site_file=fopen(path,"wb");
    if (!p118_site_file)return;
    p118_site_enabled=1;
    fputs("# P118_ORIGINAL_JIT_EXECUTED_SITE_P117_TOKEN_V1 "
          "P107_SEQ_P117_COMPILE_TOKEN_NOT_INSTRUCTION_RETIRED\n",
          p118_site_file);
    atexit(p118_site_close);
}
/* Side effect-free compiler lookup; P117 resets its token before each
 * compiled opcode, so a stale token from a prior branch cannot match.
 * compile epochs are checked against original P111 block identity.
 */
static uint32_t
p118_source_site_token(codeblock_t *block,uint32_t pc)
{
    uint32_t slot=(uint32_t)block->pnt;
    if (!p117_selected_enabled || !p117_selected_file ||
        !p111_join_enabled || !p111_join_file ||
        slot>=BLOCK_SIZE || !p118_last_p117_site ||
        p118_last_p117_pc!=pc ||
        !p111_epoch_by_slot[slot] ||
        p118_last_p117_epoch!=p111_epoch_by_slot[slot] ||
        block->pc!=p111_pc_by_slot[slot] ||
        block->phys!=p111_phys_by_slot[slot])
        return 0u;
    return p118_last_p117_site;
}
/* Invoked ONLY at JIT host-code entry for one original guest x86
 * site. The ORIGINAL P107 callback still records the real guest EBP
 * and PC; P118 adds the exactly same P107 sequence + compile token.
 * Probe overhead may perturb original scheduling: do not call this
 * a timing oracle.
 */
static void
p118_x86_executed_guest_pc(uint32_t pc,uint32_t ebp,uint32_t site)
{
    p107_x86_executed_guest_pc(pc,ebp);
    if (!p118_site_attempted)p118_site_init();
    if (!p118_site_enabled)return;
    if (!site || !p117_selected_file || !p111_join_file ||
        !p107_x86_trace_file || !p107_x86_trace_seq) {
        p118_site_no_go();
        return;
    }
    if (++p118_site_record_count>1000000ULL) {
        fputs("# P118_EVENT_CAP_REACHED_NO_COMPLETE_CLAIM\n",p118_site_file);
        p118_site_close();
        p118_site_enabled=0;
        return;
    }
    fprintf(p118_site_file,"X %llu %u %08X %llu\n",
        (unsigned long long)(p107_x86_trace_seq-1u),
        (unsigned)site,(unsigned)pc,(unsigned long long)tsc);
    if ((p118_site_record_count & 2047ULL)==0ULL)
        fflush(p118_site_file);
}
'''

# Third JIT callback argument is an IMMEDIATE in the correct native x64 ABI:
#   Win64 R8D: 41 B8 <imm32>   |   SysV AMD64 EDX: BA <imm32>
# x64 codegen's existing call() helper saves host register allocation state.
JIT_SITE= r'''        const uint32_t p118_token=p118_source_site_token(block,old_pc);
        load_param_1_32(block, old_pc);
        load_param_2_reg_32(LOAD_REG_L(REG_EBP));
#if _WIN64
        addbyte(0x41); addbyte(0xb8); /* MOV R8D, imm32 */
#else
        addbyte(0xba); /* MOV EDX, imm32 */
#endif
        addlong(p118_token);
        call(block, (uintptr_t)p118_x86_executed_guest_pc);'''

def once(src:str,anchor:str,value:str)->str:
    if src.count(anchor)!=1:
        raise ValueError('P118 exact original P117/P107 anchor absent/duplicated '+repr(anchor[:85]))
    return src.replace(anchor,value,1)

def patch(original_codegen:bytes,existing_p117_codegen:bytes)->bytes:
    if git_blob_sha1(original_codegen)!=ORIGINAL_CODEGEN_SHA:
        raise ValueError('P118 pristine original old-x64 codegen SHA pin mismatch')
    original=original_codegen.replace(b'\r\n',b'\n').decode('utf-8')
    src=existing_p117_codegen.replace(b'\r\n',b'\n').decode('utf-8')
    if P118_MARKER in src:
        raise ValueError('P118 already instrumented')
    if (P107_MARKER not in src or P111_MARKER not in src or
        P117_MARKER not in src or
        P107_CALLBACK_ANCHOR not in original and P107_CALLBACK_ANCHOR not in src):
        # P107 callback only exists in the PATCHED codegen.
        if not (P107_MARKER in src and P111_MARKER in src and
                P117_MARKER in src and P107_CALLBACK_ANCHOR in src):
            raise ValueError('P118 requires the original P107/P111/P117 source pipeline')
    if P107_CALLBACK_ANCHOR not in src:
        raise ValueError('P118 P107 exact JIT callback anchor absent')
    src=once(src,JIT_GLOBAL_ANCHOR,JIT_GLOBAL_ANCHOR+'\n'+JIT_CODE)
    src=once(src,RESET_ANCHOR,RESET_ANCHOR+'''
    p118_last_p117_site=0u;
    p118_last_p117_pc=0u;
    p118_last_p117_epoch=0u;''')
    src=once(src,SUCCESS_ANCHOR,'''    p118_last_p117_pc=pc;
    p118_last_p117_epoch=epoch;
    p118_last_p117_site=(uint32_t)p117_selected_seq;
'''+SUCCESS_ANCHOR)
    src=once(src,P107_CALLBACK_ANCHOR,JIT_SITE)
    if src.count(P117_CAPTURE_ANCHOR)!=1:
        raise ValueError('P118 cannot prove P117 capture precedes JIT emission')
    before=src.find(P117_CAPTURE_ANCHOR)
    after=src.find('const uint32_t p118_token=p118_source_site_token(block,old_pc);')
    if before<0 or after<=before or after-before>4000:
        raise ValueError('P118 original P117 captured decode not paired with JIT emission')
    return src.encode('utf-8')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--pristine-old-codegen',type=Path,required=True)
    p.add_argument('--disposable-old-codegen',type=Path,required=True)
    p.add_argument('--verify-only',action='store_true')
    args=p.parse_args()
    try:
        if args.pristine_old_codegen.resolve()==args.disposable_old_codegen.resolve():
            raise ValueError('P118 refuses to modify original OLD 86Box codegen')
        o=patch(args.pristine_old_codegen.read_bytes(),
                args.disposable_old_codegen.read_bytes())
        if not args.verify_only:
            args.disposable_old_codegen.write_bytes(o)
        print('P118_ORIGINAL_JIT_EXECUTED_SITE_COMPILER_TOKEN_PATCH_PASS '
              'source_86box_original=SHA_PINNED original_chip_modified=NO '
              'private_guest_executed=NO')
    except (OSError,UnicodeError,ValueError) as e:
        print('P118_PATCH_NO_GO '+str(e),file=sys.stderr)
        return 2
    return 0
if __name__=='__main__':raise SystemExit(main())
