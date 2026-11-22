#!/usr/bin/env python3
"""P212: original Win95 Creative trace says one allocator stop per voice.

Private owner reference: 19 Nov 2026 P007AX, Creative SBAWE.VXD,
bank1 preset7 note60: 304 port OUT words (4 voices), bank0 ROM preset7:
76 OUT words (1 voice). Prior P211 native emitted 324 and 78 respectively.
This patch changes ONLY the reconstructed synth's Win95 voice-start
sequence, never the original C EMU8000 DSP, sample bank, VST3 or RT code.
"""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: p212_original_guest_no_duplicate_stop.py PINNED_UPSTREAM")
src=Path(sys.argv[1])/"AWE32Emu/src/Synth.cpp"
text=src.read_text(encoding="utf-8")
old="""        // Silencing: the driver writes 0x00FF, not 0x0080. VTFT goes twice.
        m_core.Write(Reg::DCYSUSV, voice, 0x00FFu);
        m_core.Write(Reg::VTFT,    voice, 0x0000FFFFu);
        m_core.Write(Reg::VTFT,    voice, 0x0000FFFFu);
        m_core.Write(Reg::CVCF,    voice, 0x0000FFFFu);"""
new="""        // P212 verified against PRIVATE ORIGINAL Win95 Creative SBAWE.VXD
        // EMU8000 chip-port writes on ROM and 4-zone SF2 note60, preset7:
        // the VXD allocator has ALREADY issued DCYSUSV=00FF. A second
        // start-main DCYSUSV incorrectly adds two extra port writes per
        // voice. The P204 SF2 worklist has also ALREADY reset VTFT during
        // reservation, so the SF2 main needs one VTFT=FFFF instead of two.
        // Keep two main VTFT writes for SF1/SBK. The original C chip/DSP,
        // bank sample PCM and starting-envelope register body are untouched.
        if (!(bank && bank->version == SoundFont::Version::Sf2))
            m_core.Write(Reg::VTFT, voice, 0x0000FFFFu);
        m_core.Write(Reg::VTFT,    voice, 0x0000FFFFu);
        m_core.Write(Reg::CVCF,    voice, 0x0000FFFFu);"""
if text.count(old)!=1:
    raise SystemExit("P212: source drift, start-main duplicated-stop anchor not unique")
src.write_text(text.replace(old,new), encoding="utf-8")
print("P212_ORIGINAL_WIN95_GUEST_NO_DUPLICATE_STOP_PATCH_APPLIED")
