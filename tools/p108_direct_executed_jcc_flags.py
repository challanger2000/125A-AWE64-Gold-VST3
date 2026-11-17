#!/usr/bin/env python3
"""P108 private-only, opt-in ORIGINAL P54C OLD x64 Dynarec direct Jcc predicate probe.

Target is ONLY a disposable, already P107 JIT+interpreter-patched copy of
86Box-AWE32 v6.0-AWE32-0.93 src/codegen/codegen_x86-64.c.

At the actual *executed* Jcc's existing P107 guest-PC callback, compute
the five original Creative predicates directly from the same lazy x86
CF_SET() and ZF_SET() helpers used by the original 86Box Jcc emitter.
This does NOT depend on the next PC being captured. Every predicate is
separately checked against the next-PC observation when one exists.

No original Creative binary or guest content is bundled or uploaded.
The original EMU8000 DSP and product VST3 are not touched.
Adding a callback can perturb host scheduling; TSC is diagnostic only.
"""
from __future__ import annotations
from pathlib import Path
import argparse
import sys
import hashlib

P107_MARKER = "P107_OLD_DYNAREC_LCOD_EXECUTED_V1"
P107_INTERPRETER_MARKER = "P107_OLD_DYNAREC_INTERPRETER_FALLBACK_V2"
P108_MARKER = "P108_DIRECT_EXECUTED_X86_JCC_CF_ZF_V1"
FUNCTION_ANCHOR = "static void\np107_x86_executed_guest_pc(uint32_t guest_pc, uint32_t actual_guest_ebp)\n{"
TAIL_ANCHOR = """    if ((p107_x86_trace_count & 2047ULL) == 0ULL)
        fflush(p107_x86_trace_file);
}
"""
ADDED = r'''/* P108_DIRECT_EXECUTED_X86_JCC_CF_ZF_V1
 * These are the ACTUAL original 86Box x86_flags.h pure lazy-flag read
 * functions, used by codegen_ops_jump.h for the old Dynarec Jcc.
 * This is a direct condition observation, not an x86-next-PC guess.
 * Store a separate private raw trace so existing P107 files stay readable.
 */
static FILE *p108_direct_branch_trace;
static int p108_direct_branch_init_attempted;
static uint64_t p108_direct_branch_count;

static void
p108_close_direct_branch_trace(void)
{
    if (p108_direct_branch_trace) {
        fflush(p108_direct_branch_trace);
        fclose(p108_direct_branch_trace);
        p108_direct_branch_trace = NULL;
    }
}

static void
p108_init_direct_branch_trace(void)
{
    const char *path;
    if (p108_direct_branch_init_attempted)return;
    p108_direct_branch_init_attempted = 1;
    path = getenv("AWE64_P108_DIRECT_BRANCH_TRACE");
    if (!path || !*path || !p107_x86_probe_enabled) return;
    p108_direct_branch_trace = fopen(path, "wb");
    if (!p108_direct_branch_trace)return;
    fputs("# P108_DIRECT_EXECUTED_X86_JCC_CF_ZF_V1 "
          "same_86box_lazy_flags_as_original_jcc "
          "host_block_tsc_is_NOT_per_instruction\n",
          p108_direct_branch_trace);
    atexit(p108_close_direct_branch_trace);
}

static void
p108_record_executed_branch_predicate(uint32_t guest_pc)
{
    uint32_t off;
    int zf,cf,taken;
    if (!p107_x86_probe_enabled ||
        guest_pc < p107_lcod_guest_base) return;
    off = guest_pc - p107_lcod_guest_base;
    if (off != 0x4027u && off != 0x40cbu && off != 0x40e4u &&
        off != 0x40f1u && off != 0x4107u) return;
    if (!p108_direct_branch_init_attempted)
        p108_init_direct_branch_trace();
    if (!p108_direct_branch_trace)return;
    if (++p108_direct_branch_count > 2000000ULL) {
        fputs("# P108_EVENT_CAP_REACHED\n",p108_direct_branch_trace);
        p108_close_direct_branch_trace();
        return;
    }
    /* CF_SET and ZF_SET perform no flag mutation.  They use the
     * same lazy CPU-state sources as the original old Dynarec Jcc
     * implementation. No assumption about guest EFLAGS reconstruction.
     */
    zf = ZF_SET() ? 1 : 0;
    cf = CF_SET() ? 1 : 0;
    if      (off == 0x4027u) taken = !zf;        /* JNE */
    else if (off == 0x40cbu) taken = !cf && !zf; /* JA  */
    else if (off == 0x40e4u) taken = zf;         /* JE  */
    else if (off == 0x40f1u) taken = !cf;        /* JAE */
    else                     taken = cf;         /* JB  */
    /* p107_x86_executed_guest_pc already emitted one I event and
     * advanced p107_x86_trace_seq. The B event shares its I sequence.
     * No Creative per-event data is published.
     */
    fprintf(p108_direct_branch_trace,
            "B %llu %llu %04X %08X %d %d %d\n",
            (unsigned long long)(p107_x86_trace_seq - 1ULL),
            (unsigned long long)tsc,(unsigned)CS,
            (unsigned)guest_pc,zf,cf,taken);
    if ((p108_direct_branch_count & 2047ULL) == 0ULL)
        fflush(p108_direct_branch_trace);
}

'''


def patch(source: bytes) -> bytes:
    s=source.replace(b"\r\n",b"\n").decode("utf8")
    if P107_MARKER not in s or P107_INTERPRETER_MARKER not in s:
        raise ValueError("P107 OLD JIT + interpreter source not found")
    if P108_MARKER in s:
        raise ValueError("P108 source already instrumented")
    if s.count('#    include "x86_flags.h"')!=1:
        raise ValueError("original 86Box lazy-flag reader include not established")
    if s.count(FUNCTION_ANCHOR)!=1 or s.count(TAIL_ANCHOR)!=1:
        raise ValueError("original P107 executed-guest-PC callback anchor missing or duplicated")
    # Original emitter directly reads CF_SET / ZF_SET, so this is a
    # non-mutating observation at *actual* executed guest Jcc callback.
    s=s.replace(FUNCTION_ANCHOR,ADDED+FUNCTION_ANCHOR,1)
    s=s.replace(TAIL_ANCHOR,TAIL_ANCHOR.replace(
        "        fflush(p107_x86_trace_file);\n}",
        "        fflush(p107_x86_trace_file);\n"
        "    p108_record_executed_branch_predicate(guest_pc);\n}"),1)
    return s.encode("utf8")


def main():
    arg=argparse.ArgumentParser(description=__doc__)
    arg.add_argument("--copy-old-codegen",type=Path,required=True)
    arg.add_argument("--verify-only",action="store_true")
    a=arg.parse_args()
    try:
        source=a.copy_old_codegen.read_bytes()
        patched=patch(source)
        if not a.verify_only:a.copy_old_codegen.write_bytes(patched)
        print("P108_OLD_DYNAREC_ORIGINAL_JCC_DIRECT_FLAGS_PATCH_PASS "
              "sha256="+hashlib.sha256(patched).hexdigest()
              +" old_86box_cf_zf_unchanged=TRUE original_dsp_untouched=TRUE "
              +" original_guest_executed=FALSE")
        return 0
    except (ValueError,OSError,UnicodeError) as exc:
        print("P108_SOURCE_PIN_NO_GO "+str(exc),file=sys.stderr)
        return 2
if __name__=="__main__":
    sys.exit(main())
