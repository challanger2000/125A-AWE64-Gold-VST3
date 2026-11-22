#!/usr/bin/env python3
"""P200: original-Win95-guest-supported JOINT stereo voice trigger order.

Independent owner-original Win95/86Box register evidence recorded 25 stereo
paired starts (incl voices 29/30): BOTH hardware main voice programs complete
before EITHER volume DCYSUSV note trigger. Two independent MONO layers instead
each trigger immediately after its own main. The pinned AWE32Emu Synth.cpp
always triggered immediately. Preserve chip DSP, ROM, SF1 and mono order.
The native trace oracle is included separately; the original guest is NOT
executed on public CI.
"""
from pathlib import Path
import sys

def one(s,a,b,tag):
    n=s.count(a)
    if n!=1:raise RuntimeError(f"P200 {tag} pinned anchor mismatch {n}")
    return s.replace(a,b)

def main():
    if len(sys.argv)!=2:
        raise SystemExit("usage: p200_original_stereo_joint_trigger.py <upstream-root>")
    src=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"
    hpath,cpath=src/"Synth.h",src/"Synth.cpp"
    h,c=hpath.read_text(encoding="utf-8"),cpath.read_text(encoding="utf-8")
    if "P200 original stereo joint trigger" in h or "P200 original stereo joint trigger" in c:
        raise RuntimeError("P200 already applied")
    h=one(h,
"""    void StartVoice(int voice, uint8_t channel, uint8_t note, uint8_t velocity,
                    const SoundFont::VoiceParams& vp,
                    const SoundFont::Bank* bank, const SoundFont::Region* region);""",
"""    void StartVoice(int voice, uint8_t channel, uint8_t note, uint8_t velocity,
                    const SoundFont::VoiceParams& vp,
                    const SoundFont::Bank* bank, const SoundFont::Region* region,
                    bool deferTrigger = false);""","declaration")
    c=one(c,
"""void Synth::StartVoice(int voice, uint8_t channel, uint8_t note, uint8_t velocity,
                       const SoundFont::VoiceParams& vp,
                       const SoundFont::Bank* bank, const SoundFont::Region* region)""",
"""void Synth::StartVoice(int voice, uint8_t channel, uint8_t note, uint8_t velocity,
                       const SoundFont::VoiceParams& vp,
                       const SoundFont::Bank* bank, const SoundFont::Region* region,
                       bool deferTrigger)""","definition")
    c=one(c,
"""    m_core.Write(Reg::DCYSUSV, voice, vp.dcysusv);   // starts the note""",
"""    // P200 original stereo joint trigger: prepare BOTH voices before the
    // first note-on envelope write. Mono and SF1 keep immediate start.
    if (!deferTrigger)
        m_core.Write(Reg::DCYSUSV, voice, vp.dcysusv);   // starts the note""","trigger")
    old="""    // A preset can have several layers per note - each gets a voice.
    // The win95 family selects the voice like SBAWE.VXD (reading the chip),
    // the others by our own rule.
    for (size_t i = 0; i < regions.size(); ++i)
    {
        const int voice = vxd ? AllocateVoiceVxd(0xFFFE) : AllocateVoice();
        StartVoice(voice, channel, note, velocity, vps[i], &b, &regions[i]);
        if (vxd)
        {
            m_alloc[voice].vxdExclKey = exclKey;
            m_alloc[voice].vxdExclClass = static_cast<uint8_t>(
                regions[i].gen.Get(SoundFont::Gen::ExclusiveClass, 0));
        }
    }"""
    new="""    // P200 original stereo joint trigger: original Win95 guest trace has
    // 25 physical stereo pair starts (including voices 29/30). The VXD
    // programs both main blocks BEFORE either DCYSUSV start. A two-layer
    // MONO instrument instead triggers the first voice before programming
    // the second. The former single-voice path contradicted this evidence.
    // Track matching regions using fixed stack storage: no new RT heap.
    constexpr size_t kStereoWorklistCap = 256;
    std::array<uint8_t, kStereoWorklistCap> consumed{};
    const size_t stereoLimit = std::min(regions.size(), consumed.size());
    const auto reciprocal = [&](size_t i, size_t j) {
        const auto* x = regions[i].sample;
        const auto* y = regions[j].sample;
        if (!x || !y || x == y) return false;
        if (x->sampleType != 2 && x->sampleType != 4) return false;
        if (x->sampleLink >= b.samples.size()) return false;
        if (&b.samples[x->sampleLink] != y) return false;
        return y->sampleLink < b.samples.size()
            && &b.samples[y->sampleLink] == x
            && y->sampleType == (x->sampleType == 4 ? 2 : 4);
    };
    for (size_t i = 0; i < regions.size(); ++i)
    {
        if (i < stereoLimit && consumed[i]) continue;
        size_t pairedWith = regions.size();
        if (vxd && b.version == SoundFont::Version::Sf2
            && i < stereoLimit)
        {
            for (size_t j = i + 1; j < stereoLimit; ++j)
                if (!consumed[j] && reciprocal(i, j))
                {
                    pairedWith = j;
                    break;
                }
        }
        if (pairedWith != regions.size())
        {
            consumed[i] = consumed[pairedWith] = 1;
            // The existing Win95 voice ranking and original 86Box engine
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
            m_core.Write(Reg::DCYSUSV, second, vps[pairedWith].dcysusv);
            continue;
        }
        const int voice = vxd ? AllocateVoiceVxd(0xFFFE) : AllocateVoice();
        StartVoice(voice, channel, note, velocity, vps[i], &b, &regions[i]);
        if (vxd)
        {
            m_alloc[voice].vxdExclKey = exclKey;
            m_alloc[voice].vxdExclClass = static_cast<uint8_t>(
                regions[i].gen.Get(SoundFont::Gen::ExclusiveClass, 0));
        }
    }"""
    c=one(c,old,new,"Win95 pair worklist")
    hpath.write_text(h,encoding="utf-8")
    cpath.write_text(c,encoding="utf-8")
    print("P200_ORIGINAL_CREATIVE_STEREO_BOTH_MAIN_BEFORE_TWO_DCYSUSV_TRIGGERS_MONO_UNCHANGED_PATCHED")
if __name__=="__main__": main()
