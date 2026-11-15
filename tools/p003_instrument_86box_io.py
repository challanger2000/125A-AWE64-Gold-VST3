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
MARKER = "P003_CPU_GUEST_PHYSICAL_8_16_32_V3"
HEADER = "#define NPORTS 65536 /* PC/AT supports 64K ports */"
IN_HOOK = 'io_log("[%04X:%08X] (%i, %i, %04i) in w(%04X) = %04X\\n", CS, cpu_state.pc, in_smm, found, qfound, port, ret);'
OUT_HOOK = 'io_log("[%04X:%08X] (%i, %i, %04i) outw(%04X, %04X)\\n", CS, cpu_state.pc, in_smm, found, qfound, port, val);'
LOGGER = r'''
/* P003 V3: monitor the physical byte and 16-bit callback dispatch, NOT just
 * guest OUTW/INW. Original Creative uses byte index OUTB, 16-bit
 * and 32-bit OUTL/INL, which 86Box dispatches through the registered
 * p->outb/inb and p->outw/inw handlers. Record the effective word
 * callback address/value for BYTE OUTB exactly as in emu8k_outb.
 * BYTE INB triggers emu8k_inw, but returns only a byte, so mark that
 * value as unknown ('r') while preserving CPU-clock, port and order.
 * The source EMU8K trace independently contains full 16-bit read values.
 * Chip model, original Creative driver and sound device remain intact.
 * No original DSP source, driver or sound card handler is modified. */
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
                fputs("# P003_CPU_GUEST_PHYSICAL_8_16_32_V3 "
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
/* Exact EMU8K byte-handler calling semantics, from the SHA-pinned original:
 * outb(even,val) -> outw(even,val); outb(odd,val) -> outw(even,val<<8).
 * inb(even/odd) -> inw(even) -> only a byte returned. */
static uint8_t p003_physical_inb(uint16_t port, void *priv,
                                uint8_t (*cb)(uint16_t, void *))
{
    uint8_t value = cb(port, priv);
    p003_cpu_io_note('r', port & (uint16_t) ~1u, (uint16_t) value);
    return value;
}
static void p003_physical_outb(uint16_t port, uint8_t value, void *priv,
                              void (*cb)(uint16_t, uint8_t, void *))
{
    cb(port, value, priv);
    p003_cpu_io_note('W', port & (uint16_t) ~1u,
                     (uint16_t) ((port & 1u) ?
                         ((uint16_t) value << 8) : value));
}
static uint16_t p003_physical_inw(uint16_t port, void *priv,
                                 uint16_t (*cb)(uint16_t, void *))
{
    uint16_t value = cb(port, priv);
    p003_cpu_io_note('R', port, value);
    return value;
}
static void p003_physical_outw(uint16_t port, uint16_t value, void *priv,
                              void (*cb)(uint16_t, uint16_t, void *))
{
    cb(port, value, priv);
    p003_cpu_io_note('W', port, value);
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
    # The real AWE32/EMU8000 handler registers 16-bit callbacks.
    # Guest OUTL/INL calls enter these same handlers through io.c's
    # physical 16-bit split path. Record the actual callback event,
    # so source chip and CPU timestamps have exactly the same event
    # granularity; old INW/OUTW-only logging omitted most index OUTs.
    for before, after, n in (
        ("p->inw(port, p->priv)",
         "p003_physical_inw(port, p->priv, p->inw)", 2),
        ("p->inw(port + 2, p->priv)",
         "p003_physical_inw(port + 2, p->priv, p->inw)", 1),
        ("p->outw(port, val, p->priv)",
         "p003_physical_outw(port, val, p->priv, p->outw)", 1),
        ("p->outw(port + i, val >> (i << 3), p->priv)",
         "p003_physical_outw(port + i, val >> (i << 3), p->priv, p->outw)", 1),
        # Direct BYTE callbacks are the missing E22 index selects in V2.
        # The original EMU8000 has separate inb/outb and inw/outw handlers.
        ("p->inb(port, p->priv)",
         "p003_physical_inb(port, p->priv, p->inb)", 1),
        ("p->outb(port, val, p->priv)",
         "p003_physical_outb(port, val, p->priv, p->outb)", 1),
    ):
        if source.count(before) != n:
            raise ValueError("Unexpected original callback count: " + before)
        source = source.replace(before, after)
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
