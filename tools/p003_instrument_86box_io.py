#!/usr/bin/env python3
"""P003: opt-in guest CPU port-clock probe for the ORIGINAL pinned 86Box.
Patches ONLY src/io.c in a NEW copy of the fork (never the EMU8000 core).
Authentic guest Win95/Creative files remain private and untouched.
Recorded CPU TSC and CS:EIP are diagnostics, NOT presumed PCM frame parity.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import sys

ORIGINAL_IO_BLOB_SHA1 = "dbb3530af572426f3ef9e990d3158142c9bf841f"
MARKER = "P003_CPU_GUEST_OUTW_V1"
HEADER = "#define NPORTS 65536 /* PC/AT supports 64K ports */"
IN_HOOK = 'io_log("[%04X:%08X] (%i, %i, %04i) in w(%04X) = %04X\\n", CS, cpu_state.pc, in_smm, found, qfound, port, ret);'
OUT_HOOK = 'io_log("[%04X:%08X] (%i, %i, %04i) outw(%04X, %04X)\\n", CS, cpu_state.pc, in_smm, found, qfound, port, val);'
LOGGER = r'''
/* P003 experiment: strictly source-side CPU I/O trace ONLY.
 * The original EMU8000 chip implementation is not modified. */
static FILE *p003_cpu_io_file;
static int p003_cpu_io_initialized;
static unsigned long long p003_cpu_io_seq;
static void p003_cpu_io_close(void)
{
    if (p003_cpu_io_file) {
        fflush(p003_cpu_io_file);
        fclose(p003_cpu_io_file);
        p003_cpu_io_file = NULL;
    }
}
static void p003_cpu_io_note(char direction, uint16_t port, uint16_t value)
{
    const char *path;
    if (port != 0xe22 && port != 0xe20 && port != 0xa20 &&
        port != 0xa22 && port != 0x620 && port != 0x622)
        return;
    if (!p003_cpu_io_initialized) {
        p003_cpu_io_initialized = 1;
        path = getenv("AWE64_P003_CPU_IO_TRACE");
        if (path && *path) {
            p003_cpu_io_file = fopen(path, "wb");
            if (p003_cpu_io_file) {
                fputs("# P003_CPU_GUEST_OUTW_V1 "
                      "CPU_TSC_AND_REMAINING_CYCLES_NOT_YET_CLOCK_CALIBRATED\n",
                      p003_cpu_io_file);
                atexit(p003_cpu_io_close);
            }
        }
    }
    if (p003_cpu_io_file)
        fprintf(p003_cpu_io_file,
                "%c %llu %d %04X %08X %04X %04X %llu\n",
                direction, (unsigned long long) tsc, (int) cycles,
                (unsigned) CS, (unsigned) cpu_state.pc,
                (unsigned) port, (unsigned) value, p003_cpu_io_seq++);
}
'''

def canonical_git_bytes(raw: bytes) -> bytes:
    # GitHub Windows runners may expand tracked LF files to CRLF.
    # A Git blob is hashed from canonical LF bytes; do NOT disable the
    # SHA pin or broaden it to another 86Box revision.
    return raw.replace(b"\r\n", b"\n")

def git_blob_sha(raw: bytes) -> str:
    clean = canonical_git_bytes(raw)
    return hashlib.sha1(
        b"blob " + str(len(clean)).encode("ascii") + b"\0" + clean
    ).hexdigest()

def instrument(raw: bytes, pin: str | None = ORIGINAL_IO_BLOB_SHA1) -> bytes:
    if pin is not None and git_blob_sha(raw) != pin:
        raise ValueError("86Box original src/io.c Git blob hash differs")
    try:
        source = canonical_git_bytes(raw).decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ValueError("Source file is not UTF-8") from exc
    if MARKER in source:
        raise ValueError("Refuse to patch already-instrumented 86Box")
    for anchor in (HEADER, IN_HOOK, OUT_HOOK):
        if source.count(anchor) != 1:
            raise ValueError("Missing/duplicated original io.c anchor: " + anchor[:65])
    if (source.count('#include <86box/timer.h>') != 1 or
            source.count('#include "cpu.h"') != 1):
        raise ValueError("Pinned guest CPU/TSC header prerequisites missing")
    source = source.replace(HEADER, HEADER + "\n" + LOGGER, 1)
    source = source.replace(IN_HOOK,
        "p003_cpu_io_note('R', port, ret);\n    " + IN_HOOK, 1)
    source = source.replace(OUT_HOOK,
        "p003_cpu_io_note('W', port, val);\n    " + OUT_HOOK, 1)
    return source.encode("utf-8")

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original-io-c", type=Path, required=True)
    parser.add_argument("--output-io-c", type=Path)
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    if args.verify_only == (args.output_io_c is not None):
        parser.error("Provide --verify-only OR --output-io-c")
    try:
        source = args.original_io_c.read_bytes()
        patched = instrument(source)
        if args.output_io_c is not None:
            if args.output_io_c.resolve() == args.original_io_c.resolve():
                raise ValueError("Original 86Box source must remain untouched")
            with args.output_io_c.open("xb") as file:
                file.write(patched)
        print("P003_86BOX_CPU_IO_INSTRUMENTATION_VALIDATED "
              "original_git_blob=" + git_blob_sha(source) +
              " patched_sha256=" + hashlib.sha256(patched).hexdigest() +
              " EMU8000_core_mutated=NO source_86box_executed=NO "
              "VST3_audio_compared=NO")
        return 0
    except (OSError, ValueError) as exc:
        print("P003_86BOX_CPU_IO_PROBE_NO_GO: " + str(exc),
              file=sys.stderr)
        return 2

if __name__ == "__main__":
    sys.exit(main())
