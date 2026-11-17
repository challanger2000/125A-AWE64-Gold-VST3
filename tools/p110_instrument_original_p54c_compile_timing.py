#!/usr/bin/env python3
"""P110: ORIGINAL 86Box Pentium translator-timing instrumentation, PRIVATE opt-in.

On the SHA-pinned disposable ORIGINAL 86Box old Dynarec copy, after P107/P108:
 - patch original src/cpu/codegen_timing_pentium.c (NOT the sound DSP)
 - annotate original src/codegen/codegen_x86-64.c at the actual block/opcode
   timing translation callbacks (not guest execution).
Log the original P54C translator's carried regmask_modified, U-pipe
buffer, decode shadow, actual check_agi() results, cycle bookings.
Distinguish TRANSLATED x86 instructions from EXECUTED P107/P108 instructions.
Never claim a translation-time CPU cycle count is runtime timing or PCM.

Trace is disabled unless AWE64_P110_TIMING_TRANSLATION_TRACE is set, with
AWE64_P110_LCOD_BASE and AWE64_P110_LCOD_SELECTOR.
No Creative driver, Win95, ROM, banks, EMU8000 DSP or VST3 file changes.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import sys

PINNED_PENTIUM_TIMING_BLOB = "aa78ee1c96076eef581ce4f1b5a86ae6c47d82a4"
PINNED_ORIGINAL_OLD_JIT_BLOB = "673141762345c1195458bd840461302493f7d365"
P107 = "P107_OLD_DYNAREC_LCOD_EXECUTED_V1"
P108 = "P108_DIRECT_EXECUTED_X86_JCC_CF_ZF_V1"
MARKER = "P110_ORIGINAL_P54C_TRANSLATION_TIMING_V1"
TIMING_ANCHOR = "static int     prefixes;\n"
AGI_ANCHOR = "    return (regmask_modified & addr_regmask) & ~REGMASK_IMPL_ESP;"
BLOCK_ANCHOR = "    codegen_timing_block_start();"
OPCODE_ANCHOR = "    codegen_timing_opcode(opcode, fetchdat, op_32, op_pc);"
CODEGEN_DECL_ANCHOR = "void\ncodegen_block_start_recompile(codeblock_t *block)"
P110_TIMING = r'''
/* P110_ORIGINAL_P54C_TRANSLATION_TIMING_V1 (private, opt in).
 * Captures original source 86Box CODEGEN translator state, not executed PC.
 * Source P54C codegen logic is unchanged; this is purely observational.
 * Original compiled Win95 guest code, ROM and Creative DSP are unmodified.
 */
static FILE *p110_translator_file;
static int p110_translator_init_attempted;
static int p110_translator_enabled;
static uint32_t p110_translator_lcod_base;
static uint16_t p110_translator_lcod_cs;
static uint64_t p110_translator_seq;
static uint32_t p110_translator_current_pc;
static uint8_t p110_translator_current_opcode;
static uint32_t p110_translator_pre_mask;
static int p110_translator_pre_cycles;
static int p110_translator_pre_u;
static int p110_translator_pre_decode;
static int p110_translator_pre_decode_offset;
static uint32_t p110_translator_agi_calls;
static uint32_t p110_translator_agi_positive;
static int p110_translator_active;

static void
p110_translator_file_close(void)
{
    if (p110_translator_file) {
        fflush(p110_translator_file);
        fclose(p110_translator_file);
        p110_translator_file = NULL;
    }
}
static void
p110_translator_init(void)
{
    char *end = NULL;
    const char *path;
    const char *base;
    const char *selector;
    unsigned long long parsed;
    if (p110_translator_init_attempted)return;
    p110_translator_init_attempted = 1;
    path = getenv("AWE64_P110_TIMING_TRANSLATION_TRACE");
    base = getenv("AWE64_P110_LCOD_BASE");
    selector = getenv("AWE64_P110_LCOD_SELECTOR");
    if (!path || !*path || !base || !*base || !selector || !*selector)return;
    parsed = strtoull(base, &end, 0);
    if (!end || *end != 0 || parsed > 0xffff0000ULL)return;
    p110_translator_lcod_base = (uint32_t)parsed;
    parsed = strtoull(selector, &end, 0);
    if (!end || *end != 0 || parsed != 0x28u)return;
    p110_translator_lcod_cs = (uint16_t)parsed;
    p110_translator_file = fopen(path, "wb");
    if (!p110_translator_file)return;
    p110_translator_enabled = 1;
    fputs("# P110_ORIGINAL_P54C_TRANSLATION_TIMING_V1 "
          "COMPILE_TIME_NOT_GUEST_EXECUTION "
          "block_timing_NOT_instructions_tsc\n", p110_translator_file);
    atexit(p110_translator_file_close);
}
static int
p110_translator_matches(uint32_t pc, uint16_t selector)
{
    uint32_t off;
    if (!p110_translator_init_attempted)p110_translator_init();
    if (!p110_translator_enabled || selector != p110_translator_lcod_cs ||
        pc < p110_translator_lcod_base)return 0;
    off = pc - p110_translator_lcod_base;
    return off >= 0x4023u && off <= 0x4125u;
}
void
p110_original_p54c_translator_block_begin(uint32_t pc, uint16_t selector)
{
    p110_translator_active = 0;
    if (!p110_translator_matches(pc, selector))return;
    if (++p110_translator_seq > 500000ULL) {
        fputs("# P110_EVENT_CAP_REACHED_NO_COMPLETE_CLAIM\n",p110_translator_file);
        p110_translator_file_close();
        p110_translator_enabled = 0;
        return;
    }
    /* Snapshot *before* original block_start() resets the U pipe/decode,
     * but does not reset regmask_modified (source-verified).
     */
    fprintf(p110_translator_file,
            "S %llu %08X %08X %d %d %d %d\n",
            (unsigned long long)p110_translator_seq, (unsigned)pc,
            (unsigned)regmask_modified, u_pipe_full,
            decode_delay, decode_delay_offset, codegen_block_cycles);
}
void
p110_original_p54c_translator_opcode_before(uint32_t pc,
                                             uint16_t selector, uint8_t opcode)
{
    p110_translator_active = p110_translator_matches(pc, selector);
    if (!p110_translator_active)return;
    p110_translator_current_pc = pc;
    p110_translator_current_opcode = opcode;
    p110_translator_pre_mask = regmask_modified;
    p110_translator_pre_cycles = codegen_block_cycles;
    p110_translator_pre_u = u_pipe_full;
    p110_translator_pre_decode = decode_delay;
    p110_translator_pre_decode_offset = decode_delay_offset;
    p110_translator_agi_calls = 0;
    p110_translator_agi_positive = 0;
}
void
p110_original_p54c_translator_opcode_after(void)
{
    if (!p110_translator_active || !p110_translator_file)return;
    if (++p110_translator_seq > 500000ULL) {
        fputs("# P110_EVENT_CAP_REACHED_NO_COMPLETE_CLAIM\n",p110_translator_file);
        p110_translator_file_close();
        p110_translator_enabled = 0;
        p110_translator_active = 0;
        return;
    }
    fprintf(p110_translator_file,
            "O %llu %08X %02X %08X %08X %d %d %d %d %d %d %u %u\n",
            (unsigned long long)p110_translator_seq,
            (unsigned)p110_translator_current_pc,
            (unsigned)p110_translator_current_opcode,
            (unsigned)p110_translator_pre_mask,
            (unsigned)regmask_modified,
            p110_translator_pre_u,u_pipe_full,
            p110_translator_pre_decode,p110_translator_pre_decode_offset,
            p110_translator_pre_cycles,codegen_block_cycles,
            (unsigned)p110_translator_agi_calls,
            (unsigned)p110_translator_agi_positive);
    if ((p110_translator_seq & 2047ULL) == 0ULL)
        fflush(p110_translator_file);
    p110_translator_active = 0;
}

'''

def blob(raw: bytes) -> str:
    normalized=raw.replace(b"\r\n", b"\n")
    return hashlib.sha1(
        b"blob "+str(len(normalized)).encode()+b"\x00"+normalized).hexdigest()

def only_once(source:str, needle:str, replacement:str) -> str:
    if source.count(needle)!=1:
        raise ValueError("P110 original C source anchor missing or duplicated: "+needle[:70])
    return source.replace(needle,replacement,1)

def patch_pair(pristine_pentium:bytes,p107_p108_codegen:bytes,
               check_original_sha:bool=True)->tuple[bytes,bytes]:
    if check_original_sha and blob(pristine_pentium)!=PINNED_PENTIUM_TIMING_BLOB:
        raise ValueError("P110 ORIGINAL P54C Pentium timing SHA pin mismatch")
    timing=pristine_pentium.replace(b"\r\n",b"\n").decode("utf8")
    code=p107_p108_codegen.replace(b"\r\n",b"\n").decode("utf8")
    if P107 not in code or P108 not in code:
        raise ValueError("P110 needs full original old-x64 P107/P108 JIT instrumented codegen")
    if MARKER in timing or MARKER in code:
        raise ValueError("P110 original timing already patched")
    timing=only_once(timing,"#include <stdio.h>","#include <stdio.h>\n#include <stdlib.h>")
    timing=only_once(timing,TIMING_ANCHOR,TIMING_ANCHOR+P110_TIMING)
    timing=only_once(timing,AGI_ANCHOR,
'''    {
        uint32_t original_agi = (regmask_modified & addr_regmask) & ~REGMASK_IMPL_ESP;
        if (p110_translator_active && p110_translator_file) {
            ++p110_translator_agi_calls;
            if (original_agi != 0u)++p110_translator_agi_positive;
        }
        return original_agi;
    }''')
    code=only_once(code,CODEGEN_DECL_ANCHOR,
'''/* P110: original P54C *translation* observability; no audio/DSP mutation. */
extern void p110_original_p54c_translator_block_begin(uint32_t, uint16_t);
extern void p110_original_p54c_translator_opcode_before(uint32_t, uint16_t, uint8_t);
extern void p110_original_p54c_translator_opcode_after(void);
'''+CODEGEN_DECL_ANCHOR)
    code=only_once(code,BLOCK_ANCHOR,
'''    p110_original_p54c_translator_block_begin(cpu_state.pc, (uint16_t)CS);
'''+BLOCK_ANCHOR)
    code=only_once(code,OPCODE_ANCHOR,
'''    p110_original_p54c_translator_opcode_before(old_pc, (uint16_t)CS, opcode);
'''+OPCODE_ANCHOR+'''
    p110_original_p54c_translator_opcode_after();''')
    return timing.encode("utf8"),code.encode("utf8")

def main()->int:
    arg=argparse.ArgumentParser(description=__doc__)
    arg.add_argument("--original-pentium-timing",type=Path,required=True)
    arg.add_argument("--copy-pentium-timing",type=Path,required=True)
    arg.add_argument("--copy-old-codegen",type=Path,required=True)
    arg.add_argument("--verify-only",action="store_true")
    a=arg.parse_args()
    try:
        original=a.original_pentium_timing.read_bytes()
        copy=a.copy_pentium_timing.read_bytes()
        if blob(original)!=blob(copy):
            raise ValueError("P110 disposable P54C timing source already modified")
        if a.original_pentium_timing.resolve()==a.copy_pentium_timing.resolve():
            raise ValueError("refuse to modify original 86Box Pentium timing source")
        timing,code=patch_pair(original,a.copy_old_codegen.read_bytes())
        if not a.verify_only:
            a.copy_pentium_timing.write_bytes(timing)
            a.copy_old_codegen.write_bytes(code)
        print("P110_ORIGINAL_PENTIUM_P54C_COMPILE_TIMING_SOURCE_PASS "
              "pentium_blob="+blob(original)+
              " no_guest_executed=TRUE no_tsc_parity=TRUE original_emu8k_dsp_unchanged=TRUE")
        return 0
    except (OSError,ValueError,UnicodeError) as exc:
        print("P110_SOURCE_PIN_NO_GO "+str(exc),file=sys.stderr)
        return 2
if __name__=="__main__":
    sys.exit(main())
