#!/usr/bin/env python3
"""Extend the SHA-pinned 86Box V4 src/io.c research logger with guest RAM reads.

Append COMMENT records for the last physical CCCA read (0x0A22), preserving
ALL V4 event lines, E22/620/A20/A22 chip callbacks and original DSP source.
Only memory already translated through readlookup2 is observed. No guest writes.
Never publish the resulting private Win95 trace or original driver contents.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import sys

MARKER='P003_V5_ROW_STATE'
V4='P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK'
ANCHOR='static void p003_cpu_io_close(void)'
AFTER='''                    (unsigned) ECX, (unsigned) EDX);'''
READER=r'''
/* P003 V5: read only already-translated *RAM*, never a guest MMIO range.
 * A 32-byte VoiceRow is confirmed for all 491 CCCA helper calls in V4.
 * The V4 port event is unchanged; extra lines are comments for old parsers.
 * f[9] (ret0) remains an UNVERIFIED hint until original opcode checking. */
static int p003_v5_read_ram(uint32_t linear, void *dest, unsigned width)
{
    uintptr_t lookup;
    if (!width || width>4u || linear>0xffffffffu-width+1u ||
        (linear & 4095u)+width>4096u) return 0;
    lookup=readlookup2[linear >> 12];
    if (lookup==(uintptr_t)-1 || lookup==0) return 0;
    memcpy(dest,(const void *)(lookup+(uintptr_t)linear),width);
    return 1;
}
static int p003_v5_linear(uint32_t segment_base, uint32_t offset,
                          int delta, uint32_t *linear)
{
    int64_t address=(int64_t)(uint64_t)segment_base+
                    (int64_t)(uint64_t)offset+(int64_t)delta;
    if (address<0 || address>0xffffffffLL) return 0;
    *linear=(uint32_t)address;
    return 1;
}
static void p003_v5_guest_row(char direction, uint16_t port,
                              uint32_t ret0, unsigned long long io_seq)
{
    uint32_t row, frame, addr;
    uint32_t sample_end=0xffffffffu;
    uint32_t score_m04=0xffffffffu, score_m0c=0xffffffffu;
    uint32_t score_m14=0xffffffffu;
    uint16_t marker=0xffffu;
    uint8_t penalty=0xffu, valid=0u;
    if (!p003_cpu_io_file || !p003_guest_callstack_enabled ||
        direction!='R' || port!=0x0a22u || ret0==0xffffffffu)
        return;
    /* The source x86 algorithm uses DS:EBX for its per-voice row and SS:EBP
     * for the score stack. Do not assume a nonzero segment base is impossible. */
    if (!p003_v5_linear(cpu_state.seg_ds.base,EBX,0,&row) ||
        !p003_v5_linear(cpu_state.seg_ss.base,EBP,0,&frame))
        return;
    if (p003_v5_linear(0,row,0,&addr) &&
        p003_v5_read_ram(addr,&sample_end,4))valid|=0x01u;
    if (p003_v5_linear(0,row,-10,&addr) &&
        p003_v5_read_ram(addr,&marker,2))valid|=0x02u;
    if (p003_v5_linear(0,row,16,&addr) &&
        p003_v5_read_ram(addr,&penalty,1))valid|=0x04u;
    if (p003_v5_linear(0,frame,-4,&addr) &&
        p003_v5_read_ram(addr,&score_m04,4))valid|=0x08u;
    if (p003_v5_linear(0,frame,-12,&addr) &&
        p003_v5_read_ram(addr,&score_m0c,4))valid|=0x10u;
    if (p003_v5_linear(0,frame,-20,&addr) &&
        p003_v5_read_ram(addr,&score_m14,4))valid|=0x20u;
    fprintf(p003_cpu_io_file,
            "# P003_V5_ROW_STATE %llu %llu %08X %08X %08X %02X "
            "%08X %04X %02X %08X %08X %08X\n",
            io_seq,(unsigned long long)tsc,(unsigned)ret0,
            (unsigned)EBX,(unsigned)EBP,(unsigned)valid,
            (unsigned)sample_end,(unsigned)marker,(unsigned)penalty,
            (unsigned)score_m04,(unsigned)score_m0c,(unsigned)score_m14);
}
/* Original LCOD+0x3D44 begins "push ebp; mov ebp,esp; push esi".
 * This is the I/O HELPER EBP, not the calling allocator EBP.
 * [EBP] is the saved parent frame; [EBP-4] is the saved parent ESI.
 * Earlier V5_ROW_STATE scores addressed the helper frame and must NOT be
 * interpreted as allocator's current/previous/best score.
 * V5_ROW_STATE remains unmodified for backwards compatibility. */
static void p003_v5_guest_caller(char direction, uint16_t port,
                                uint32_t ret0, unsigned long long io_seq)
{
    uint32_t frame=0, caller=0xffffffffu, esi=0xffffffffu, addr;
    uint32_t current=0xffffffffu, previous=0xffffffffu;
    uint32_t best=0xffffffffu, mode=0xffffffffu;
    uint8_t next0d=0xffu, next2d=0xffu;
    unsigned valid=0;
    if (!p003_cpu_io_file || !p003_guest_callstack_enabled ||
        direction!='R' || port!=0x0a22u || ret0==0xffffffffu)
        return;
    if (!p003_v5_linear(cpu_state.seg_ss.base,EBP,0,&frame))return;
    if (p003_v5_read_ram(frame,&caller,4) && caller>EBP &&
        caller-EBP<=65536u && !(caller&3u))valid|=1u;
    if (p003_v5_linear(0,frame,-4,&addr) &&
        p003_v5_read_ram(addr,&esi,4))valid|=2u;
    if (valid&1u) {
        if (p003_v5_linear(cpu_state.seg_ss.base,caller,-4,&addr) &&
            p003_v5_read_ram(addr,&current,4))valid|=4u;
        if (p003_v5_linear(cpu_state.seg_ss.base,caller,-12,&addr) &&
            p003_v5_read_ram(addr,&previous,4))valid|=8u;
        if (p003_v5_linear(cpu_state.seg_ss.base,caller,-20,&addr) &&
            p003_v5_read_ram(addr,&best,4))valid|=16u;
    }
    if ((valid&2u) &&
        p003_v5_linear(cpu_state.seg_ds.base,esi,24,&addr) &&
        p003_v5_read_ram(addr,&mode,4))valid|=32u;
    if (p003_v5_linear(cpu_state.seg_ds.base,EBX,-19,&addr) &&
        p003_v5_read_ram(addr,&next0d,1))valid|=64u;
    /* LOOK-AHEAD to the NEXT scan row: original LCOD+411A does EBX-=32.
     * Therefore [EBX_next+0x2D] == [EBX_current+0x0D].
     * The current-row +0x2D would be wrong for this next-row observation. */
    if (p003_v5_linear(cpu_state.seg_ds.base,EBX,13,&addr) &&
        p003_v5_read_ram(addr,&next2d,1))valid|=128u;
    fprintf(p003_cpu_io_file,
            "# P003_V5_CALLER_FRAME %llu %llu %08X %08X %08X %08X "
            "%08X %02X %08X %08X %08X %08X %02X %02X\n",
            io_seq,(unsigned long long)tsc,(unsigned)ret0,
            (unsigned)EBX,(unsigned)EBP,(unsigned)caller,(unsigned)esi,
            valid,(unsigned)current,(unsigned)previous,(unsigned)best,
            (unsigned)mode,(unsigned)next0d,(unsigned)next2d);
}
'''

def instrument_v5(raw:bytes)->bytes:
    try: src=raw.replace(b'\r\n',b'\n').decode('utf-8')
    except UnicodeDecodeError as exc: raise ValueError('Not UTF-8 V4 source') from exc
    if MARKER in src: raise ValueError('V5 already inserted')
    for name,count in ((V4,1),(ANCHOR,1),(AFTER,1),
                       ('p003_guest_stack_returns(retn)',1),
                       ('lookup=readlookup2[linear >> 12]',1),
                       ('#include <86box/mem.h>',1),
                       ("p003_cpu_io_note('R', port, value)",1)):
        if src.count(name)!=count:
            raise ValueError(f'Unexpected V4 source anchor {name[:50]}: count={src.count(name)}')
    src=src.replace(ANCHOR,READER+'\n'+ANCHOR,1)
    src=src.replace(AFTER, AFTER+'''
            p003_v5_guest_row(direction,port,retn[0],
                               p003_cpu_io_seq-1u);
            p003_v5_guest_caller(direction,port,retn[0],
                                p003_cpu_io_seq-1u);''',1)
    # Deliberately do not alter the format of the 17-field original V4 lines.
    return src.encode('utf-8')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--instrumented-v4-io-c',type=Path,required=True)
    ap.add_argument('--output-v5-io-c',type=Path,required=True)
    args=ap.parse_args()
    if args.instrumented_v4_io_c.resolve()==args.output_v5_io_c.resolve():
        raise ValueError('Never modify original V4 source in place')
    data=instrument_v5(args.instrumented_v4_io_c.read_bytes())
    with args.output_v5_io_c.open('xb') as f:f.write(data)
    print('P003_V5_READONLY_ROW_PROBE_SOURCE_VALIDATED original_emu8000_core_unchanged=YES original_v4_port_lines_unchanged=YES 86box_build_executed=NO')

if __name__=='__main__':
    try:main()
    except (OSError,ValueError) as exc:
        print('P003_V5_ROW_INSTRUMENT_NO_GO:',exc,file=sys.stderr)
        sys.exit(2)
