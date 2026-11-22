#!/usr/bin/env python3
"""P199 original-SF2 stereo linked sample fix; original EMU8000 and SF1 untouched.
Linked left/right sample headers are format-level input evidence, not a
substitute for still-required original Creative Win95 driver register traces.
"""
from pathlib import Path
import sys

def one(src,old,new,label):
    n=src.count(old)
    if n!=1: raise RuntimeError(f"P199 {label} anchor mismatch: {n}")
    return src.replace(old,new)

def main():
    if len(sys.argv)!=2: raise SystemExit("usage: patch.py <upstream-root>")
    root=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"
    hp,cp=root/"SoundFont.h",root/"SoundFont.cpp"
    h,c=hp.read_text(encoding="utf-8"),cp.read_text(encoding="utf-8")
    if "P199 linked SF2 stereo" in h or "P199 linked SF2 stereo" in c:
        raise RuntimeError("P199 patch already applied")
    h=one(h,"        bool     inRom = false;",
"""        bool     inRom = false;
        // P199 linked SF2 stereo: 2=right, 4=left, 1=mono, 0x8000=ROM.
        uint16_t sampleLink = 0;
        uint16_t sampleType = 1;""","sample fields")
    c=one(c,"""            const uint16_t type = RdU16(p + 44);
            s.inRom = (type & 0x8000) != 0 || (!s.name.empty() && s.name[0] == '*');""",
"""            const uint16_t type = RdU16(p + 44);
            s.sampleLink = RdU16(p + 42);
            s.sampleType = type & 0x7FFF;
            s.inRom = (type & 0x8000) != 0 || (!s.name.empty() && s.name[0] == '*');""","shdr parse")
    c=one(c,"""        if (!bank.samples.empty() && bank.samples.back().name == "EOS")
            bank.samples.pop_back();""",
"""        if (!bank.samples.empty() && bank.samples.back().name == "EOS")
            bank.samples.pop_back();
        // Reject invalid reciprocal pairs before a voice can reference them.
        for (size_t i = 0; i < bank.samples.size(); ++i)
        {
            const Sample& s = bank.samples[i];
            if (s.sampleType != 2 && s.sampleType != 4) continue;
            if (s.sampleLink >= bank.samples.size() || s.sampleLink == i)
            {
                bank.errorMessage = "Invalid SF2 stereo sample link";
                return bank;
            }
            const Sample& mate = bank.samples[s.sampleLink];
            if (mate.sampleType != (s.sampleType == 4 ? 2 : 4)
                || mate.sampleLink != i
                || mate.sampleRate != s.sampleRate
                || mate.end < mate.start || s.end < s.start
                || mate.end - mate.start != s.end - s.start)
            {
                bank.errorMessage = "Mismatched SF2 stereo sample pair";
                return bank;
            }
        }""","stereo validation")
    c=one(c,"""            out.push_back(std::move(r));""",
"""            // P199 linked SF2 stereo: even ONE mapped half must activate
            // both sample sources on two original EMU8000 hardware voices.
            // Never duplicate the mate if an active zone explicitly maps it.
            const Sample& selected = samples[iz.sampleId];
            const bool linked = version == Version::Sf2
                && (selected.sampleType == 2 || selected.sampleType == 4)
                && selected.sampleLink < samples.size();
            bool explicitMate = false;
            if (linked)
                for (const Zone& other : in.zones)
                    if (other.sampleId == static_cast<int>(selected.sampleLink)
                        && key >= other.keyLo && key <= other.keyHi
                        && velocity >= other.velLo && velocity <= other.velHi)
                    {
                        explicitMate = true;
                        break;
                    }
            out.push_back(r);
            if (linked && !explicitMate)
            {
                Region mate = r;
                mate.sample = &samples[selected.sampleLink];
                mate.gen.Set(Gen::SampleID,
                             static_cast<int16_t>(selected.sampleLink));
                out.push_back(std::move(mate));
            }""","select zone")
    c=one(c,"""    if (sf1)
        vp.patchPan = g.Get(Gen::Pan, 64);
    else
        // SF2: -500 = fully left, +500 = fully right; the EMU8000 is reversed.
        vp.patchPan = std::clamp(64 + static_cast<int>(
            std::lround(g.Get(Gen::Pan, 0) * 127.0 / 1000.0)), 0, 127);""",
"""    if (sf1)
        vp.patchPan = g.Get(Gen::Pan, 64);
    else if (s.sampleType == 4)
        vp.patchPan = 0; // linked stereo LEFT at the original chip
    else if (s.sampleType == 2)
        vp.patchPan = 127; // linked stereo RIGHT at the original chip
    else
        // Mono SF2 keeps the previous documented pan conversion.
        vp.patchPan = std::clamp(64 + static_cast<int>(
            std::lround(g.Get(Gen::Pan, 0) * 127.0 / 1000.0)), 0, 127);""","pan conversion")
    hp.write_text(h,encoding="utf-8")
    cp.write_text(c,encoding="utf-8")
    print("P199_LINKED_SF2_STEREO_TWO_ORIGINAL_CHIP_VOICES_SF1_UNCHANGED_PATCHED")
if __name__=="__main__":main()
