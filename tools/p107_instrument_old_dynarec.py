#!/usr/bin/env python3
"""P107: opt-in *executed* guest x86 instruction observer in the pinned OLD x64 86Box Dynarec.

The user's Windows-95 P003 fork was built using CMake preset 'regular':
NEW_DYNAREC=OFF. Its C++/C source is 86Box-AWE32 v6.0-AWE32-0.93.
Do not instrument src/codegen_new for that original runtime.

This script only patches a fresh COPY of src/codegen/codegen_x86-64.c.
It neither embeds nor distributes Creative Win95/VXD/ROM/SF2/PCM content.
A JIT-generated call is emitted solely for original guest instructions
in an explicitly opt-in LCOD address window, so the callback records
EXECUTED guest PCs (not speculative instructions encountered at compile time).

IMPORTANT: The generated CALL may cause observer effects; virtual TSC inside
a JIT block is not a per-instruction-clock oracle. Product VST3 untouched.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sys

PINNED_UPSTREAM_SOURCE_BLOB = "673141762345c1195458bd840461302493f7d365"
MARKER = "P107_OLD_DYNAREC_LCOD_EXECUTED_V1"
INCLUDE_ANCHOR = '#    include "codegen_ops_x86-64.h"'
GENERATE_ANCHOR = """    codegen_accumulate(ACCREG_cycles, -codegen_block_cycles);
    codegen_block_cycles = 0;

    if ((op_table == x86_dynarec_opcodes"""
INSTRUMENTATION = r'''
/* P107 OLD-DYNAREC-ONLY opt-in, not part of an original chip, VxD or VST3.
 * The original 86Box v6.0-AWE32-0.93 "regular" build selects this path.
 * Logging is emitted in the machine code actually executed by 86Box.
 * An opcode/guest-address gate excludes unrelated Win95 routines.
 *
 * Instrumentation overhead occurs in the host and can change guest event
 * scheduling; this is NOT itself a proof of identical original TSC/PCM.
 * No guest memory writes; no code patching; raw output remains PRIVATE.
 */
#define P107_OLD_DYNAREC_LCOD_EXECUTED_V1 1
static FILE *p107_x86_trace_file;
static int p107_x86_probe_initialized;
static int p107_x86_probe_enabled;
static uint32_t p107_lcod_guest_base;
static uint16_t p107_lcod_guest_cs;
static uint64_t p107_x86_trace_seq;
static uint64_t p107_x86_trace_count;

static void
p107_x86_trace_close(void)
{
    if (p107_x86_trace_file) {
        fflush(p107_x86_trace_file);
        fclose(p107_x86_trace_file);
        p107_x86_trace_file = NULL;
    }
}

static int
p107_parse_u32(const char *text, uint32_t *out)
{
    char *end = NULL;
    unsigned long long x;
    if (!text || !*text) return 0;
    x = strtoull(text, &end, 0);
    if (end == text || !end || *end != '\0' || x > 0xffffffffULL)
        return 0;
    *out = (uint32_t)x;
    return 1;
}

static void
p107_x86_probe_init(void)
{
    const char *path;
    const char *base_str;
    const char *cs_str;
    uint32_t base, selector=0x28;
    if (p107_x86_probe_initialized)return;
    p107_x86_probe_initialized = 1;
    path = getenv("AWE64_P107_EXECUTED_X86_TRACE");
    base_str = getenv("AWE64_P107_LCOD_BASE");
    cs_str = getenv("AWE64_P107_LCOD_SELECTOR");
    if (!path || !*path || !base_str ||
        !p107_parse_u32(base_str, &base) ||
        (cs_str && !p107_parse_u32(cs_str, &selector)) ||
        selector > 0xffffu || base > 0xffff0000u)
        return;
    p107_x86_trace_file = fopen(path,"wb");
    if (!p107_x86_trace_file)return;
    p107_lcod_guest_base = base;
    p107_lcod_guest_cs = (uint16_t)selector;
    p107_x86_probe_enabled=1;
    fputs("# P107_OLD_DYNAREC_EXECUTED_LCOD_V1 guest_instruction_pc "
          "tsc_at_block_observer_not_instruction_cycle_oracle "
          "ebp_minus_18_read_only_invalid_is_unknown\n",
          p107_x86_trace_file);
    atexit(p107_x86_trace_close);
}

static int
p107_x86_in_original_lcod(uint32_t old_pc, uint8_t opcode)
{
    uint32_t off;
    if (!p107_x86_probe_initialized)p107_x86_probe_init();
    if (!p107_x86_probe_enabled || CS != p107_lcod_guest_cs ||
        old_pc < p107_lcod_guest_base)
        return 0;
    off = old_pc - p107_lcod_guest_base;
    if (off < 0x4023u || off > 0x4125u)return 0;
    /* Five original Jcc bytes from the independently SHA-checked LCOD.
     * Do not silently run a different VxD after a changed load base.
     * The branch opcode is the opcode at the guest *instruction start*.
     */
    if ((off == 0x4027u && opcode != 0x75u) ||
        (off == 0x40cbu && opcode != 0x77u) ||
        (off == 0x40e4u && opcode != 0x74u) ||
        (off == 0x40f1u && opcode != 0x73u) ||
        (off == 0x4107u && opcode != 0x72u)) {
        fatal("P107: unexpected original LCOD Jcc guest opcode at %08x\n",
              (unsigned)old_pc);
    }
    return 1;
}

/* Return 0 for unmapped/unreliable stack memory, never synthesize a DWORD.
 * Reading through the pinned 86Box readlookup2 cache is non-mutating and
 * never exercises the Creative-EMU8000 I/O handler.
 */
static int
p107_guest_stack_minus18(uint32_t guest_ebp, uint32_t *value)
{
    uint64_t linear64;
    uint32_t linear;
    uintptr_t lookup;
    if (guest_ebp < 0x18u)return 0;
    linear64 = (uint64_t)ss + (uint64_t)guest_ebp - 0x18u;
    if (linear64 > 0xffffffffULL)return 0;
    linear = (uint32_t)linear64;
    if ((linear & 4095u) > 4092u)return 0;
    lookup = readlookup2[linear >> 12];
    if (!lookup || lookup == (uintptr_t)-1)return 0;
    memcpy(value, (const void *)(lookup + (uintptr_t)linear), 4u);
    return 1;
}

/* This is called by the executed host JIT code, never by the x86
 * translation-time interpreter. The passed EBP is read from the guest
 * register by the emitted old-dynarec host instructions, not by guessing
 * from stale CPU-state memory.
 */
static void
p107_x86_executed_guest_pc(uint32_t guest_pc, uint32_t actual_guest_ebp)
{
    uint32_t local18 = 0xffffffffu;
    int known = 0;
    if (!p107_x86_trace_file)return;
    if (++p107_x86_trace_count > 2000000ULL) {
        fputs("# P107_EVENT_CAP_REACHED no_complete_trace_claim\n",
              p107_x86_trace_file);
        p107_x86_trace_close();
        return;
    }
    if (guest_pc == p107_lcod_guest_base + 0x4027u)
        known = p107_guest_stack_minus18(actual_guest_ebp, &local18);
    fprintf(p107_x86_trace_file,
            "I %llu %llu %04X %08X %08X %d %08X\n",
            (unsigned long long)p107_x86_trace_seq++,
            (unsigned long long)tsc, (unsigned)CS, (unsigned)guest_pc,
            (unsigned)actual_guest_ebp, known, (unsigned)local18);
    if ((p107_x86_trace_count & 2047ULL) == 0ULL)
        fflush(p107_x86_trace_file);
}
'''
INSERT_BEFORE = """    /* P107: Record *executed* guest x86 PCs in the old x64 Dynarec only.
     * The additional host CALL is intentionally not priced as guest cycles;
     * compare source CPU/PCM separately to detect observer effects.
     */
    if (p107_x86_in_original_lcod(old_pc, opcode)) {
        load_param_1_32(block, old_pc);
        load_param_2_reg_32(LOAD_REG_L(REG_EBP));
        call(block, (uintptr_t)p107_x86_executed_guest_pc);
    }

"""


def git_blob_sha1(raw: bytes) -> str:
    normalized=raw.replace(b"\r\n",b"\n")
    return hashlib.sha1(
        b"blob "+str(len(normalized)).encode()+b"\x00"+normalized
    ).hexdigest()


def patch(raw: bytes, source_blob_sha: str | None = PINNED_UPSTREAM_SOURCE_BLOB) -> bytes:
    if source_blob_sha is not None and git_blob_sha1(raw)!=source_blob_sha:
        raise ValueError("Pinned upstream OLD dynarec codegen source hash mismatch")
    source=raw.replace(b"\r\n",b"\n").decode("utf-8")
    if MARKER in source:
        raise ValueError("Already instrumented (refuse double injection)")
    if source.count(INCLUDE_ANCHOR)!=1:
        raise ValueError("Missing or duplicated old Dynarec header anchor")
    if source.count(GENERATE_ANCHOR)!=1:
        raise ValueError("Missing or duplicated original generator timing anchor")
    if source.count("void\ncodegen_generate_call(")!=1:
        raise ValueError("The original old Dynarec compiler structure changed")
    if source.count("    codegen_accumulate(ACCREG_cycles, -codegen_block_cycles);")!=1:
        raise ValueError("Old Dynarec timing sites unexpectedly modified")
    if "USE_NEW_DYNAREC" in source:
        raise ValueError("Unexpected NEW-DYNAREC source")
    source=source.replace(INCLUDE_ANCHOR,INCLUDE_ANCHOR+"\n"+INSTRUMENTATION,1)
    source=source.replace(GENERATE_ANCHOR,
                          "    codegen_accumulate(ACCREG_cycles, -codegen_block_cycles);\n"
                          "    codegen_block_cycles = 0;\n\n"
                          + INSERT_BEFORE
                          + "    if ((op_table == x86_dynarec_opcodes",1)
    return source.encode("utf-8")


def main() -> int:
    arg=argparse.ArgumentParser(description=__doc__)
    arg.add_argument("--original-codegen",type=Path,required=True)
    arg.add_argument("--patched-output",type=Path)
    arg.add_argument("--verify-only",action="store_true")
    a=arg.parse_args()
    if a.verify_only == (a.patched_output is not None):
        arg.error("Use exactly one of --verify-only or --patched-output")
    try:
        raw=a.original_codegen.read_bytes()
        result=patch(raw)
        if a.patched_output:
            if a.patched_output.resolve()==a.original_codegen.resolve():
                raise ValueError("Cannot overwrite original fork")
            with a.patched_output.open("xb") as f:f.write(result)
        print("P107_PINNED_86BOX_OLD_DYNAREC_EXECUTION_OBSERVER_SOURCE_PASS "
              "upstream_blob="+git_blob_sha1(raw)
              +" patched_sha256="+hashlib.sha256(result).hexdigest()
              +" original_chip_mutated=FALSE"
              +" original_guest_executed=FALSE"
              +" scheduler_parity=NOT_VERIFIED")
        return 0
    except (OSError, ValueError, UnicodeError) as exc:
        print("P107_OLD_DYNAREC_PROBE_NO_GO "+str(exc),file=sys.stderr)
        return 2


if __name__=="__main__":
    sys.exit(main())
