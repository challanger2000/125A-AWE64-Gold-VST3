#!/usr/bin/env python3
"""P201 source-backed original Win95 SBAWE.VXD adjacent stereo pair routing.

Real owner Creative Win95 25-pair sequence: stereo needs adjacent hardware
voices; LEFT programmed on base+1 then RIGHT on base, both main blocks done
before DCYSUSV triggers RIGHT(base) then LEFT(base+1). Reconstruct cold and
ordinary non-ROM pair selection from independent original C++23 VXD port.
This patch does NOT claim full dynamic allocator parity (per-voice priority
flag and original device+0x18 state not yet fully modelled in this Synth).
"""
from pathlib import Path
import sys

def one(s,old,new,name):
    n=s.count(old)
    if n!=1:raise RuntimeError(f"P201 pinned {name} anchor count {n}")
    return s.replace(old,new)

def main():
    if len(sys.argv)!=2:
        raise SystemExit("usage: p201_original_stereo_adjacent_pair.py <upstream-root>")
    root=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"
    header,source=root/"Synth.h",root/"Synth.cpp"
    h,c=header.read_text(encoding="utf-8"),source.read_text(encoding="utf-8")
    if "P201 original Creative stereo pair" in c:
        raise RuntimeError("P201 already applied")
    h=one(h,"    int  AllocateVoiceVxd(uint16_t state);",
"""    // P201 original Creative stereo pair allocator; returns physical base.
    // A linked pair occupies consecutive hardware voices base/base+1.
    int  AllocateStereoPairVxd(uint16_t state);
    int  AllocateVoiceVxd(uint16_t state);""","declaration")
    c=one(c,"int Synth::AllocateVoiceVxd(uint16_t state)",
"""// P201 original Creative stereo pair: source-backed LCOD+0x3FE8 mode 1.
// Scan descending, score neighboring physical voice rows, and reserve one
// adjacent pair. This is separate from three-lane MONO AllocateVoiceVxd.
// Full VXD parity still needs the original device+0x18 and voice+0x1a bit
// beyond the present Synth VoiceAlloc: their unverified cases stay NO-GO.
int Synth::AllocateStereoPairVxd(uint16_t state)
{
    constexpr uint32_t kInvalidScore = 0x000FFFFFu;
    int romPlaying = 0;
    for (int v = 0; v < kVxdVoices; ++v)
        if (m_alloc[v].vxdRom && m_alloc[v].vxdState != 0x0FFE)
            ++romPlaying;
    const bool protectRom = romPlaying <= 2;

    uint32_t previous = kInvalidScore;
    uint32_t best = kInvalidScore;
    int chosenBase = -1;
    for (int n = kVxdVoices; n > 0; --n)
    {
        const int voice = n - 1;
        const VoiceAlloc& a = m_alloc[voice];
        const bool excluded =
            a.vxdState >= 0x2000
            || (protectRom && a.vxdRom)
            || (n < kVxdVoices && (
                m_alloc[n].vxdState >= 0x2000
                || (protectRom && m_alloc[n].vxdRom)));
        if (excluded)
        {
            previous = kInvalidScore;
            continue;
        }

        uint32_t current = m_core.ReadDriver(Reg::VTFT, voice) >> 16;
        if (a.vxdState != 0x0FFE)
        {
            current += (a.vxdState & 0x1000) ? 0x1200u : 0x0200u;
            if ((a.vxdState & 0xFF) == 0xFF)
                current += 0x0300u;
            else if (!(m_core.ReadDriver(Reg::DCYSUSV, voice)
                       & Emu8000::kDcysusvRelease))
                current += 0x1300u;
        }
        if (a.vxdEndAddr != 0
            && (m_core.ReadDriver(Reg::CCCA, voice)
                & Emu8000::kCccaAddressMask) >= a.vxdEndAddr)
            current = 0;

        const uint32_t pairSum = previous + current;
        if (n < kVxdVoices && pairSum == 0u)
        {
            chosenBase = voice;
            break;
        }
        const uint32_t priority =
            pairSum + 2u * std::max(previous, current);
        if (n < kVxdVoices && priority <= best)
        {
            best = priority;
            chosenBase = voice;
        }
        previous = current;
    }
    if (chosenBase < 0 || chosenBase + 1 >= kVxdVoices)
        return -1;
    for (int v = chosenBase; v <= chosenBase + 1; ++v)
    {
        VoiceAlloc& a = m_alloc[v];
        a.vxdState = state;
        a.vxdEndAddr = 0;
        a.vxdRom = false;
        a.inUse = false;
        a.heldBySustain = false;
        m_core.Write(Reg::DCYSUSV, v, 0x00FFu);
    }
    return chosenBase;
}

int Synth::AllocateVoiceVxd(uint16_t state)""","pair allocator")
    before="""            // The existing Win95 voice ranking and original 86Box engine
            // remain unchanged. Allocation ordering/pair adjacency against
            // original VXD is still a separate independent release gate.
            const int first = AllocateVoiceVxd(0xFFFE);
            const int second = AllocateVoiceVxd(0xFFFE);
            StartVoice(first, channel, note, velocity, vps[i], &b,
                       &regions[i], true);
            StartVoice(second, channel, note, velocity, vps[pairedWith],
                       &b, &regions[pairedWith], true);
            m_alloc[first].vxdExclKey = exclKey;
            m_alloc[first].vxdExclClass = static_cast<uint8_t>(
                regions[i].gen.Get(SoundFont::Gen::ExclusiveClass, 0));
            m_alloc[second].vxdExclKey = exclKey;
            m_alloc[second].vxdExclClass = static_cast<uint8_t>(
                regions[pairedWith].gen.Get(SoundFont::Gen::ExclusiveClass, 0));
            // Trigger after BOTH complete register blocks, as verified in
            // original owner Win95 Creative paired note-on captures.
            m_core.Write(Reg::DCYSUSV, first, vps[i].dcysusv);
            m_core.Write(Reg::DCYSUSV, second, vps[pairedWith].dcysusv);"""
    after="""            // P201 original Creative stereo pair:
            // left sound source on physical base+1, right on base. The
            // original worklist writes both main blocks (LEFT then RIGHT)
            // before either start, then starts RIGHT(base), LEFT(base+1).
            const size_t left = regions[i].sample->sampleType == 4
                ? i : pairedWith;
            const size_t right = left == i ? pairedWith : i;
            const int base = AllocateStereoPairVxd(0xFFFE);
            if (base < 0)
                continue;  // original allocator could not reserve a pair
            StartVoice(base + 1, channel, note, velocity, vps[left], &b,
                       &regions[left], true);
            StartVoice(base, channel, note, velocity, vps[right], &b,
                       &regions[right], true);
            m_alloc[base + 1].vxdExclKey = exclKey;
            m_alloc[base + 1].vxdExclClass = static_cast<uint8_t>(
                regions[left].gen.Get(SoundFont::Gen::ExclusiveClass, 0));
            m_alloc[base].vxdExclKey = exclKey;
            m_alloc[base].vxdExclClass = static_cast<uint8_t>(
                regions[right].gen.Get(SoundFont::Gen::ExclusiveClass, 0));
            m_core.Write(Reg::DCYSUSV, base, vps[right].dcysusv);
            m_core.Write(Reg::DCYSUSV, base + 1, vps[left].dcysusv);"""
    c=one(c,before,after,"coupled physical routing and reverse trigger")
    header.write_text(h,encoding="utf-8")
    source.write_text(c,encoding="utf-8")
    print("P201_ORIGINAL_CREATIVE_ADJACENT_STEREO_BASE_RIGHT_TRIGGER_THEN_LEFT_PASS")
if __name__=="__main__":main()
