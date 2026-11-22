#!/usr/bin/env python3
"""P204 original Creative Win95 SF2 worklist: reserve ALL before ANY main."""
from pathlib import Path
import sys
def one(s,a,b,label):
    n=s.count(a)
    if n!=1:raise RuntimeError(f"P204 {label} pinned anchor {n}")
    return s.replace(a,b)
def main():
    if len(sys.argv)!=2:raise SystemExit("patch.py <source root>")
    f=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"/"Synth.cpp"
    s=f.read_text(encoding="utf-8")
    if "P204_CREATIVE_ALL_RESERVATIONS_FIRST" in s:raise RuntimeError("duplicate P204")
    marker="    // A preset can have several layers per note - each gets a voice."
    body="""
#ifndef P204_LEGACY_INTERLEAVED_ALLOCATOR
    // P204_CREATIVE_ALL_RESERVATIONS_FIRST: original SBAWE.VXD
    // first reserves EVERY physical voice from the note worklist, THEN
    // performs note-main register writes and finally envelope triggers.
    // Its first pass selects linked SF2 stereo; second pass other monos.
    // SF1/SBK and DOS retain their separately checked previous semantics.
    if (vxd && b.version == SoundFont::Version::Sf2 &&
        regions.size() <= 256u)
    {
        struct WorkVoice
        {
            size_t source = 0;
            int physical = -1;
            bool pairHead = false;
        };
        std::array<WorkVoice, 256> work{};
        std::array<uint8_t, 256> consumed{};
        size_t workCount = 0;
        const auto isLinked = [&](size_t left, size_t right) {
            const auto* l = regions[left].sample;
            const auto* r = regions[right].sample;
            return l && r && l != r && l->sampleType == 4 &&
                r->sampleType == 2 &&
                l->sampleLink < b.samples.size() &&
                r->sampleLink < b.samples.size() &&
                &b.samples[l->sampleLink] == r &&
                &b.samples[r->sampleLink] == l;
        };
        // Original VXD worklist pairs RIGHT with LEFT irrespective of
        // source zone ordering, ahead of any independent mono source.
        for (size_t right = 0; right < regions.size(); ++right)
        {
            if (consumed[right] || !regions[right].sample ||
                regions[right].sample->sampleType != 2)
                continue;
            size_t left = regions.size();
            for (size_t j = 0; j < regions.size(); ++j)
                if (!consumed[j] && j != right && isLinked(j, right))
                { left = j; break; }
            if (left == regions.size()) continue;
            consumed[left] = consumed[right] = 1;
            const int base = AllocateStereoPairVxd(0xFFFE);
            if (base < 0) continue;
            // VXD physical row+0x17 and device+0x18 must be updated
            // during reservation, before next layer chooses a voice.
            SetVxdPhysicalRomClass(base + 1,
                vps[left].sampleStart < 0x200000u);
            SetVxdPhysicalRomClass(base,
                vps[right].sampleStart < 0x200000u);
            work[workCount++] = {left, base + 1, true};
            work[workCount++] = {right, base, false};
        }
        for (size_t i = 0; i < regions.size(); ++i)
        {
            if (consumed[i]) continue;
            const int voice = AllocateVoiceVxd(0xFFFE);
            if (voice < 0) continue;
            SetVxdPhysicalRomClass(voice,
                vps[i].sampleStart < 0x200000u);
            work[workCount++] = {i, voice, false};
        }
        // NO first-main or NoteOn envelope until all reservation
        // prologues have executed through the actual original 86Box chip.
        const auto classify = [&](const WorkVoice& v) {
            m_alloc[v.physical].vxdExclKey = exclKey;
            m_alloc[v.physical].vxdExclClass = static_cast<uint8_t>(
                regions[v.source].gen.Get(SoundFont::Gen::ExclusiveClass, 0));
        };
        for (size_t i = 0; i < workCount; ++i)
        {
            const WorkVoice& a = work[i];
            if (a.pairHead)
            {
                const WorkVoice& r = work[i + 1];
                StartVoice(a.physical, channel, note, velocity,
                    vps[a.source], &b, &regions[a.source], true);
                StartVoice(r.physical, channel, note, velocity,
                    vps[r.source], &b, &regions[r.source], true);
                classify(a);
                classify(r);
                // Original VXD: LEFT upper and RIGHT base main before
                // EITHER trigger, trigger RIGHT(base), then LEFT(base+1).
                m_core.Write(Reg::DCYSUSV, r.physical,
                    vps[r.source].dcysusv);
                m_core.Write(Reg::DCYSUSV, a.physical,
                    vps[a.source].dcysusv);
                ++i;
            }
            else
            {
                // Independent mono: main and immediate original trigger.
                StartVoice(a.physical, channel, note, velocity,
                    vps[a.source], &b, &regions[a.source]);
                classify(a);
            }
        }
        return;
    }
#endif
"""
    s=one(s,marker,body+marker,"StartLayers entry")
    f.write_text(s,encoding="utf-8")
    print("P204_ORIGINAL_SF2_VXD_ALL_VOICES_RESERVED_BEFORE_MAIN_PATCHED")
if __name__=="__main__":main()
