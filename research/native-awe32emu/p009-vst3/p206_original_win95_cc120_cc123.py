#!/usr/bin/env python3
"""P206 original Win95 SBAWE.VXD CC120/CC123 release and sustain semantics."""
from pathlib import Path
import sys

def one(s,a,b,n):
    k=s.count(a)
    if k!=1: raise RuntimeError(f"P206 pinned {n} anchor count={k}")
    return s.replace(a,b)

def main():
    if len(sys.argv)!=2: raise SystemExit("usage: p206.py <pinned upstream>")
    path=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"/"Synth.cpp"
    s=path.read_text(encoding="utf-8")
    if "P206_ORIGINAL_VXD_CC120_CC123" in s:
        raise RuntimeError("P206 already applied")
    s=one(s,"""    case 120:
        for (int i = 0; i < NoteVoices(); ++i)
            if (m_alloc[i].channel == channel) KillVoice(i);
        break;

    case 123:
        for (int i = 0; i < NoteVoices(); ++i)
            if (m_alloc[i].inUse && m_alloc[i].channel == channel) ReleaseVoice(i);
        break;""","""    // P206_ORIGINAL_VXD_CC120_CC123: SBAWE.VXD +0x6C82 visits the
    // original physical row marker, not only a generic inUse flag.
    // CC120 All Sound Off releases both EMU8000 envelopes immediately,
    // ignoring pedal, while CC123 All Notes Off defers on sustain ON
    // (low marker byte 0xFF) and releases only on pedal OFF.
    // Preserve the separately reconstructed DOS/SDK control path.
    case 120:
#ifdef P206_NEGATIVE_LEGACY_CC
        for (int i = 0; i < NoteVoices(); ++i)
            if (m_alloc[i].channel == channel) KillVoice(i);
#else
        if (m_core.DriverVariant() == Awe32::Driver::Win95)
        {
            for (int i = 0; i < kVxdVoices; ++i)
            {
                const VoiceAlloc& a = m_alloc[i];
                if (a.vxdState == 0x0FFEu || (a.vxdState >> 8) != channel)
                    continue;
                ReleaseVoice(i);
            }
        }
        else
            for (int i = 0; i < NoteVoices(); ++i)
                if (m_alloc[i].channel == channel) KillVoice(i);
#endif
        break;

    case 123:
#ifdef P206_NEGATIVE_LEGACY_CC
        for (int i = 0; i < NoteVoices(); ++i)
            if (m_alloc[i].inUse && m_alloc[i].channel == channel)
                ReleaseVoice(i);
#else
        if (m_core.DriverVariant() == Awe32::Driver::Win95)
        {
            for (int i = 0; i < kVxdVoices; ++i)
            {
                VoiceAlloc& a = m_alloc[i];
                if (a.vxdState == 0x0FFEu || (a.vxdState >> 8) != channel)
                    continue;
                if (ch.sustain)
                {
                    // Original VXD low key marker 0xFF: no chip writes.
                    a.heldBySustain = true;
                    a.vxdState = static_cast<uint16_t>(
                        (static_cast<unsigned>(channel) << 8) | 0xFFu);
                }
                else
                    ReleaseVoice(i);
            }
        }
        else
            for (int i = 0; i < NoteVoices(); ++i)
                if (m_alloc[i].inUse && m_alloc[i].channel == channel)
                    ReleaseVoice(i);
#endif
        break;""","vxd cc120/cc123")
    path.write_text(s,encoding="utf-8")
    print("P206_ORIGINAL_WIN95_CC120_RELEASE_CC123_SUSTAIN_DEFER_PATCHED")

if __name__=="__main__": main()
