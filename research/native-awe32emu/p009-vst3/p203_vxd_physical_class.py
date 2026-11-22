#!/usr/bin/env python3
"""P203: original Win95 SBAWE.VXD device mode, physical flags, stereo rank."""
from pathlib import Path
import sys

def one(s,a,b,label):
    n=s.count(a)
    if n!=1: raise RuntimeError(f"P203 {label} pinned anchor count {n}")
    return s.replace(a,b)

def main():
    if len(sys.argv)!=2: raise SystemExit("usage: patch.py <pinned checkout>")
    root=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"
    hp,cp=root/"Synth.h",root/"Synth.cpp"
    h,c=hp.read_text(encoding="utf-8"),cp.read_text(encoding="utf-8")
    if "P203_ORIGINAL_VXD_COUNT" in c: raise RuntimeError("P203 applied twice")
    h=one(h,"private:\n    struct LoadedBank",
"""private:
    friend struct P203VoiceStateQA;
    struct LoadedBank""","QA friend")
    h=one(h,"        bool     vxdRom = false;",
"""        bool     vxdRom = false;
        // Original row+0x17 persists through voice reuse, NOT active notes.
        bool     vxdBlocked = false;
        // Original row+0x1a bit0 adds 0xC00 in MODE1 pair scoring.
        bool     vxdPairPenalty = false;""","original per-voice metadata")
    h=one(h,"    Emu8000Core m_core;",
"""    // Original Win95 VXD device+0x18, initialized to 32 physical voices.
    uint32_t m_vxdModeCount = 32u;
    Emu8000Core m_core;""","mutable device+0x18")
    h=one(h,"    int  AllocateStereoPairVxd(uint16_t state);",
"""    void SetVxdPhysicalRomClass(int voice, bool rom) noexcept;
    int  AllocateStereoPairVxd(uint16_t state);""","class transition declaration")

    c=one(c,"int Synth::AllocateStereoPairVxd(uint16_t state)",
"""// P203_ORIGINAL_VXD_COUNT: LCOD+0x557D and LCOD+0x4C60.
void Synth::SetVxdPhysicalRomClass(int voice, bool rom) noexcept
{
    VoiceAlloc& a = m_alloc[voice];
    if (a.vxdBlocked == rom) return;
    if (rom) ++m_vxdModeCount;
    else --m_vxdModeCount;
    a.vxdBlocked = rom;
}

int Synth::AllocateStereoPairVxd(uint16_t state)""","mode transition helper")
    c=one(c,"""    int romPlaying = 0;
    for (int v = 0; v < kVxdVoices; ++v)
        if (m_alloc[v].vxdRom && m_alloc[v].vxdState != 0x0FFE)
            ++romPlaying;
    const bool protectRom = romPlaying <= 2;""",
"""#ifdef P203_NEGATIVE_BAD_PROTECTION
    const bool protectRom = false; // test-only original-bug mutant
#else
    const bool protectRom = m_vxdModeCount <= 2u;
#endif""","stereo original device+0x18 conditional")
    c=one(c,"""        const bool excluded =
            a.vxdState >= 0x2000
            || (protectRom && a.vxdRom)
            || (n < kVxdVoices && (
                m_alloc[n].vxdState >= 0x2000
                || (protectRom && m_alloc[n].vxdRom)));""",
"""        // Original VXD excludes current key>=0x2000, but NEVER vetoes a
        // candidate because upper neighbor's key>=0x2000. Only neighbor
        // row+0x17 protected flag is checked when device+0x18<=2.
        const bool excluded =
            a.vxdState >= 0x2000
            || (protectRom && a.vxdBlocked)
            || (n < kVxdVoices && protectRom && m_alloc[n].vxdBlocked);""","mode1 neighbor logic")
    c=one(c,"""        const uint32_t priority =
            pairSum + 2u * std::max(previous, current);""",
"""        const uint32_t priority =
            pairSum + 2u * std::max(previous, current)
            + (a.vxdPairPenalty ? 0x0C00u : 0u);""","mode1 original 0x1a pair penalty")
    c=one(c,"""    // While at most two ROM voices are playing, ROM voices are not taken.
    // The flag means "playing a ROM sample now" - it is cleared when the voice
    // is released; kept after the note it made every voice of a ROM bank
    // untouchable and all notes of Georgia after 18 s fell on voice 0.
    int romPlaying = 0;
    for (int v = 0; v < kVxdVoices; ++v)
        if (m_alloc[v].vxdRom && m_alloc[v].vxdState != 0xFFE) ++romPlaying;
    const bool protectRom = (romPlaying <= 2);""",
"""    // Source-based original device+0x18 and persistent physical row+0x17,
    // NOT the previous guess from the number of active ROM notes.
#ifdef P203_NEGATIVE_BAD_PROTECTION
    const bool protectRom = false; // test-only old allocator
#else
    const bool protectRom = m_vxdModeCount <= 2u;
#endif""","mono original device+0x18 conditional")
    c=one(c,"            if (protectRom && a.vxdRom) continue;",
"""            if (protectRom && a.vxdBlocked) continue;""","mono persistent protected class")
    c=one(c,"    if (chosen < 0) chosen = (cand >= 0) ? cand : 0;",
"""    // Original allocator has an explicit NO-CANDIDATE result.
    // Never steal protected voice0 by fallback.
    if (chosen < 0) chosen = cand;
    if (chosen < 0) return -1;""","mono no-candidate")
    c=one(c,"""        const int voice = vxd ? AllocateVoiceVxd(0xFFFE) : AllocateVoice();
        StartVoice(voice, channel, note, velocity, vps[i], &b, &regions[i]);""",
"""        const int voice = vxd ? AllocateVoiceVxd(0xFFFE) : AllocateVoice();
        if (voice < 0) continue; // original allocator can refuse a note
        StartVoice(voice, channel, note, velocity, vps[i], &b, &regions[i]);""","prevent negative hardware voice indexing")
    c=one(c,"""    VoiceAlloc& a = m_alloc[voice];
    a = VoiceAlloc{};
    a.inUse = true;""",
"""    VoiceAlloc& a = m_alloc[voice];
    const bool previousBlocked = a.vxdBlocked;
    const bool previousPenalty = a.vxdPairPenalty;
    a = VoiceAlloc{};
    // The original physical row metadata survives per-note reinitializing.
    a.vxdBlocked = previousBlocked;
    a.vxdPairPenalty = previousPenalty;
    a.inUse = true;""","preserve voice+0x17/+0x1a after note start")
    c=one(c,"""        a.vxdState = static_cast<uint16_t>((channel << 8) | note);
        a.vxdRom = sampleInRom;""",
"""        a.vxdState = static_cast<uint16_t>((channel << 8) | note);
        a.vxdRom = sampleInRom;
        // Original source classifies PHYSICAL sample start >=0x200000
        // as RAM regardless of a SoundFont name.
        SetVxdPhysicalRomClass(voice, vp.sampleStart < 0x200000u);""","actual physical sample class")
    hp.write_text(h,encoding="utf-8")
    cp.write_text(c,encoding="utf-8")
    print("P203_ORIGINAL_VXD_DEVICE_COUNT_PERSISTENT_VOICE_CLASS_PENALTY_PATCHED")

if __name__=="__main__":main()
