#!/usr/bin/env python3
"""Add opt-in, source-pinned block/time diagnostics to ORIGINAL 86Box Pentium CPU.

Only a COPY of src/cpu/386_dynarec.c is patched; no original Creative VXD,
EMU8000 DSP, Win95 guest files, or native VST3 implementation are modified.

The original I/O logger records guest TSC and remaining cycles at EMU8000
callbacks. This extension observes the CPU block that executes each callback
and records *before/after* the upstream code's TSC reconciliation.
It is research evidence, NOT an autonomous plugin scheduler.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sys

# Git blob of original turican0/86Box-AWE32 src/cpu/386_dynarec.c,
# pinned to v6.0-AWE32-0.93. Never silently accept another upstream CPU core.
ORIGINAL_SHA1 = "e9765a423b5d675337aab33fd060204e62dc20d7"
MARKER = "P003_DYNBLK_BLOCK_CLOCK_V1"

ANCHOR_MACRO = "#define CPU_BLOCK_END() cpu_block_end = 1"
ANCHOR_PRE_EXEC = """            oldtsc           = tsc;
            tsc_old          = tsc;
            if (cpu_force_interpreter || cpu_override_dynarec ||  (!CACHE_ON())) /*Interpret block*/"""
ANCHOR_POST_EXEC = """                exec386_dynarec_dyn();
            }

            if (cpu_init) {"""
ANCHOR_POST_TSC = """            if (cycdiff > 0) {
                if (TIMER_VAL_LESS_THAN_VAL(timer_target, (uint64_t) tsc))
                    timer_process();
            }

#    ifdef USE_GDBSTUB"""
ANCHOR_CACHED = """        inrecomp = 1;
        code();"""
ANCHOR_RECOMPILE = """        codegen_block_start_recompile(block);
        codegen_in_recompile = 1;"""
ANCHOR_NEW = """        codegen_block_init(phys_addr);

        while (!cpu_block_end) {"""

LOGGER = r'''
/* P003_DYNBLK_BLOCK_CLOCK_V1
 * Original upstream CPU execution semantics are unchanged.
 * Raw guest CPU budgets/timestamps only. Caller supplies LCOD base; never
 * assume a previous VM session's relocation is valid in this session.
 * Mapped block path: 0=new code block, 1=recompile, 2=cached,
 * 3=interpreter/other fallback; not a claim about per-instruction timings.
 */
static FILE *p003_dynblk_file;
static int p003_dynblk_initialized;
static uint32_t p003_dynblk_lcod_base;
static unsigned long long p003_dynblk_sequence;
static int p003_dynblk_path;

static void p003_dynblk_close(void)
{
    if (p003_dynblk_file) {
        fflush(p003_dynblk_file);
        fclose(p003_dynblk_file);
        p003_dynblk_file = NULL;
    }
}
static void p003_dynblk_clock(char stage, uint32_t from, uint32_t to,
                              uint64_t tsc0, uint64_t tsc1,
                              int32_t cycles0, int32_t cycles1, int path)
{
    if (!p003_dynblk_initialized) {
        const char *log_path = getenv("AWE64_P003_DYNAREC_BLOCK_TRACE");
        const char *base_text = getenv("AWE64_P003_DYNAREC_LCOD_BASE");
        p003_dynblk_initialized = 1;
        if (log_path && *log_path && base_text && *base_text) {
            char *end = NULL;
            unsigned long base = strtoul(base_text, &end, 16);
            if (end && *end == '\0' && end != base_text &&
                base <= 0xffffffffUL) {
                p003_dynblk_lcod_base = (uint32_t)base;
                p003_dynblk_file = fopen(log_path, "wb");
                if (p003_dynblk_file) {
                    fputs("# P003_DYNBLK_BLOCK_CLOCK_V1 "
                          "STAGE_SEQ_PATH_FROM_TO_TSC0_TSC1_CYC0_CYC1\n",
                          p003_dynblk_file);
                    atexit(p003_dynblk_close);
                }
            }
        }
    }
    if (!p003_dynblk_file) return;
    /* Include the original I/O helpers and allocator. Other guest execution
     * is deliberately excluded to keep the private diagnostic bounded. */
    const uint32_t off0 = from - p003_dynblk_lcod_base;
    const uint32_t off1 = to - p003_dynblk_lcod_base;
    if (!((off0 >= 0x3c00u && off0 <= 0x4200u) ||
          (off1 >= 0x3c00u && off1 <= 0x4200u)))
        return;
    fprintf(p003_dynblk_file,
            "%c %llu %d %08X %08X %llu %llu %d %d\n",
            stage, p003_dynblk_sequence++, path, from, to,
            (unsigned long long)tsc0, (unsigned long long)tsc1,
            (int)cycles0, (int)cycles1);
}
'''

def git_blob_sha(raw: bytes) -> str:
    raw=raw.replace(b"\r\n",b"\n")
    return hashlib.sha1(b"blob "+str(len(raw)).encode()+b"\0"+raw).hexdigest()

def instrument(raw: bytes, expected_sha: str | None=ORIGINAL_SHA1) -> bytes:
    if expected_sha is not None and git_blob_sha(raw)!=expected_sha:
        raise ValueError("Original pinned 86Box 386_dynarec.c SHA1 mismatch")
    try:
        code=raw.replace(b"\r\n",b"\n").decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ValueError("Not UTF-8 upstream 86Box CPU source") from exc
    if MARKER in code:
        raise ValueError("Refuse repeated P003 Dynarec instrumentation")
    anchors=(ANCHOR_MACRO,ANCHOR_PRE_EXEC,ANCHOR_POST_EXEC,
             ANCHOR_POST_TSC,ANCHOR_CACHED,ANCHOR_RECOMPILE,ANCHOR_NEW)
    for anchor in anchors:
        if code.count(anchor)!=1:
            raise ValueError(f"Original Dynarec anchor missing or duplicated: {anchor[:65]}")
    code=code.replace(ANCHOR_MACRO,ANCHOR_MACRO+"\n"+LOGGER,1)
    code=code.replace(ANCHOR_CACHED,
                      "        p003_dynblk_path = 2;\n"+ANCHOR_CACHED,1)
    code=code.replace(ANCHOR_RECOMPILE,
                      "        p003_dynblk_path = 1;\n"+ANCHOR_RECOMPILE,1)
    code=code.replace(ANCHOR_NEW,
                      "        p003_dynblk_path = 0;\n"+ANCHOR_NEW,1)
    code=code.replace(ANCHOR_PRE_EXEC, """            oldtsc           = tsc;
            tsc_old          = tsc;
            const uint32_t p003_dynblk_entry_pc = cs + cpu_state.pc;
            p003_dynblk_path = 3;
            if (cpu_force_interpreter || cpu_override_dynarec ||  (!CACHE_ON())) /*Interpret block*/""",1)
    code=code.replace(ANCHOR_POST_EXEC, """                exec386_dynarec_dyn();
            }
            p003_dynblk_clock('X',p003_dynblk_entry_pc,cs+cpu_state.pc,
                              oldtsc,tsc,oldcyc,cycles,p003_dynblk_path);

            if (cpu_init) {""",1)
    code=code.replace(ANCHOR_POST_TSC, """            if (cycdiff > 0) {
                if (TIMER_VAL_LESS_THAN_VAL(timer_target, (uint64_t) tsc))
                    timer_process();
            }
            p003_dynblk_clock('S',p003_dynblk_entry_pc,cs+cpu_state.pc,
                              oldtsc,tsc,oldcyc,cycles,p003_dynblk_path);

#    ifdef USE_GDBSTUB""",1)
    return code.encode("utf-8")

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--original-cpu-c",type=Path,required=True)
    ap.add_argument("--output-cpu-c",type=Path)
    ap.add_argument("--verify-only",action="store_true")
    args=ap.parse_args()
    if args.verify_only == (args.output_cpu_c is not None):
        ap.error("Choose --verify-only OR --output-cpu-c")
    original=args.original_cpu_c.read_bytes()
    result=instrument(original)
    if args.output_cpu_c is not None:
        if args.original_cpu_c.resolve()==args.output_cpu_c.resolve():
            raise ValueError("Original pinned CPU file must remain untouched")
        with args.output_cpu_c.open("xb") as f:f.write(result)
    print("P003_DYNBLK_ORIGINAL_CPU_SOURCE_PASS "
          "git_blob="+git_blob_sha(original)+" "
          "patched_sha256="+hashlib.sha256(result).hexdigest()+" "
          "EMU8000_core_modified=NO original_guest_modified=NO "
          "86Box_executed=NO actual_CPU_block_clock_measured=NO")
    return 0

if __name__=="__main__":
    try:sys.exit(main())
    except (OSError,ValueError) as exc:
        print("P003_DYNBLK_SOURCE_NO_GO:",exc,file=sys.stderr)
        sys.exit(2)
