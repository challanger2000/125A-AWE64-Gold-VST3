#!/usr/bin/env python3
"""Port the already authored/validated P175 12-control REAL EMU8000 overlay.
Applies to the exact pinned turican0 AWE32Emu Synth translation layer ONLY.
No original 86Box chip C code or Creative base register math is replaced.
All GUI controls initialized to zero; P022 owner PCM remains bit-identical.
"""
from pathlib import Path
import sys

def unique_replace(text, old, new, file_name):
    if text.count(old) != 1:
        raise RuntimeError("P165 original upstream anchor changed: "+file_name)
    return text.replace(old, new)

def main():
    if len(sys.argv)!=2:
        raise SystemExit("Usage: apply_p165_original_synth_overlays.py <pinned-dir>")
    root=Path(sys.argv[1])/"AWE32Emu"/"src"
    hdr=root/"Synth.h"
    src=root/"Synth.cpp"
    h=hdr.read_text(encoding="utf-8")
    s=src.read_text(encoding="utf-8")
    if "m_guiControls" in h or "void Synth::SetGuiControl(" in s:
        raise RuntimeError("P165 overlay unexpectedly already present")
    h=unique_replace(h,
       "    explicit Synth(uint32_t sampleRate);",
       """    explicit Synth(uint32_t sampleRate);
    // Original P175 proven non-destructive performance modifiers.
    // This is per-VST3-instance; zero is exact native Creative PCM.
    static constexpr int kGuiControlCount=12;
    void SetGuiControl(int index,float normalized) noexcept;
    void SetGuiSendManual(int index,bool manual) noexcept;
    float GuiControl(int index) const noexcept;""","Synth.h ctor")
    h=unique_replace(h,"    Emu8000Core m_core;",
       "    Emu8000Core m_core;\n    std::array<float,kGuiControlCount> m_guiControls{};\n    std::array<bool,2> m_guiSendManual{};",
       "Synth.h core")
    setter="""void Synth::SetGuiControl(int index,float normalized) noexcept
{
    if(index>=0 && index<kGuiControlCount && std::isfinite(normalized))
        m_guiControls[static_cast<size_t>(index)] =
            std::clamp(normalized,0.f,1.f);
}
void Synth::SetGuiSendManual(int index,bool manual) noexcept
{
    if(index>=0 && index<2)m_guiSendManual[static_cast<size_t>(index)]=manual;
}
float Synth::GuiControl(int index) const noexcept
{
    return index>=0 && index<kGuiControlCount
       ?m_guiControls[static_cast<size_t>(index)] : 0.f;
}

"""
    s=unique_replace(s,"void Synth::ControlChange(",setter+"void Synth::ControlChange(",
                     "Synth.cpp controls")
    old="""    for (const SoundFont::Region& r : regions)
        vps.push_back(SoundFont::MakeVoiceParams(
            b, r, note, velocity, m_banks[bankIndex].dramBase, kRomPoolBase,
            m_core.DriverVariant()));"""
    new="""    for (const SoundFont::Region& r : regions)
    {
        auto v=SoundFont::MakeVoiceParams(
            b, r, note, velocity, m_banks[bankIndex].dramBase, kRomPoolBase,
            m_core.DriverVariant());
        // User-editable, deliberately non-destructive performance overlay.
        // All zero = exact existing SF2-to-register behaviour. No bank edits.
        // 0 = the actual patch; increasing the overlay SHORTENS its time
        // towards the fastest hardware rate. Never replace the patch by an
        // unrelated absolute time immediately above zero.
        auto timeRate = [](int original,float value) -> uint16_t {
            return static_cast<uint16_t>(original+
                int(std::lround((127-original)*value)));
        };
        auto replaceLow = [](uint16_t registerValue,uint16_t low) -> uint16_t {
            return static_cast<uint16_t>((registerValue&0xFF00u)|(low&0x7Fu));
        };
        auto replaceHigh = [](uint16_t registerValue,int high) -> uint16_t {
            return static_cast<uint16_t>((registerValue&0x00FFu)|
                (static_cast<unsigned>(std::clamp(high,0,127))<<8));
        };
        auto shortenAttack = [&](uint16_t& delay,uint16_t& attack,float amount) {
            if(amount<=0.f)return;
            // Delay precedes attack and hold precedes decay on the chip.
            // Both must reach zero for a held pad to become a short click.
            const int ticks=std::max(0,0x8000-int(delay));
            delay=static_cast<uint16_t>(0x8000-int(std::lround(ticks*(1.f-amount))));
            attack=replaceHigh(replaceLow(attack,timeRate(attack&127,amount)),
                timeRate((attack>>8)&127,amount));
        };
        shortenAttack(v.envvol,v.atkhldv,m_guiControls[0]);
        if(m_guiControls[1]>0.f) v.dcysusv=replaceLow(v.dcysusv,timeRate(v.dcysusv&127,m_guiControls[1]));
        if(m_guiControls[2]>0.f) v.dcysusv=replaceHigh(v.dcysusv,
            int(std::lround(((v.dcysusv>>8)&127)*(1.f-m_guiControls[2]))));
        if(m_guiControls[3]>0.f) {
            // StartVoice uses 0x40 for a zero volume-release rate outside
            // the DOS families. Interpolate from the rate actually played,
            // otherwise 1% could lengthen that fallback to tens of seconds.
            const int original=v.releaseRate ? v.releaseRate :
                (Awe32::IsDosLike(m_core.DriverVariant()) ? 0 : 0x40);
            v.releaseRate=static_cast<uint8_t>(timeRate(original,m_guiControls[3]));
        }
        shortenAttack(v.envval,v.atkhld,m_guiControls[4]);
        if(m_guiControls[5]>0.f) v.dcysus=replaceLow(v.dcysus,timeRate(v.dcysus&127,m_guiControls[5]));
        if(m_guiControls[6]>0.f) v.dcysus=replaceHigh(v.dcysus,
            int(std::lround(((v.dcysus>>8)&127)*(1.f-m_guiControls[6]))));
        if(m_guiControls[7]>0.f) v.releaseModRate=static_cast<uint8_t>(timeRate(v.releaseModRate,m_guiControls[7]));
        // EMU8000 has one modulation envelope routed through PEFE. Retain
        // existing positive/negative patch routes and pitch modulation. When
        // the patch has no filter route, introduce an actual hardware filter
        // sweep progressively with the overlay, with its peak at the patch's
        // original cutoff (not a second software filter or extra DSP).
        const float filterEdit=std::max({m_guiControls[4],m_guiControls[5],
                                        m_guiControls[6],m_guiControls[7]});
        if(filterEdit>0.f && (v.pefe&0xFFu)==0) {
            // The real EMU8000 filter has a nonnegative cutoff floor. A
            // full six-octave PEFE sweep cannot be compensated when the
            // original patch cutoff is already near that floor. Without
            // limiting depth, the base register clips at zero and the
            // modulation PEAK actually becomes much brighter than the
            // user's original sound (verified with a dark synthetic SF2
            // and genuine ROM-backed 86Box PCM). Limit the added route to
            // the available original-cutoff headroom instead.
            const int cutoff=(v.ifatn>>8)&255;
            const int depth=std::min(int(std::lround(96.f*filterEdit)),cutoff/2);
            v.pefe=static_cast<uint16_t>((v.pefe&0xFF00u)|depth);
            // Signed PEFE depth 127 spans ~6 octaves (~245 cutoff steps);
            // two native register steps per depth are a conservative bound.
            v.ifatn=static_cast<uint16_t>(((cutoff-2*depth)<<8)|(v.ifatn&255));
        }
        // Span the remaining register range from the patch, so a nonzero
        // initial setting does not clip the entire upper half of a knob.
        if(m_guiControls[8]>0.f) {
            const int cutoff=(v.ifatn>>8)&0xFF;
            v.ifatn=static_cast<uint16_t>((std::max(0,cutoff-int(std::lround(
                m_guiControls[8]*cutoff)))<<8)|(v.ifatn&0xFF));
        }
        if(m_guiControls[9]>0.f) {
            const int q=static_cast<int>((v.ccca>>28)&0xF);
            const auto improved=static_cast<uint32_t>(std::min(15,
                q+int(std::lround(m_guiControls[9]*(15-q)))));
            v.ccca=(v.ccca&0x0FFFFFFFu)|(improved<<28);
        }
        // Unlike envelope/filter overlays, the reverb and chorus SEND
        // controls have absolute chip-register semantics after explicit
        // activation (0=0, 100=255). Do not ALSO add any send value here.
        // In ORIGINAL mode, do not touch original SoundFont generators at
        // all: even when the host has a nonzero but inactive knob value.
        vps.push_back(v);
    }"""
    s=unique_replace(s,old,new,"Synth.cpp Original StartLayers")
    # Only an explicit UI/host SEND edit switches from faithful original
    # Creative/SF2 send levels to absolute 0..255 hardware send values.
    # Untouched patches retain BIT-EXACT original PCM; explicit 0% is the
    # ACTUAL hardware register value zero, including any channel CC91/93.
    s=unique_replace(s,
        """    const int chorus = std::clamp(
        static_cast<int>((vp.csl >> Emu8000::kChorusShift) & 0xFF) + chChorus, 0, 255);""",
        """    const int chorus = m_guiSendManual[1]
        ? std::clamp(int(std::lround(m_guiControls[11]*255.f)),0,255)
        : std::clamp(static_cast<int>((vp.csl >> Emu8000::kChorusShift) & 0xFF)
                     + chChorus,0,255);""",
        "Synth.cpp ORIGINAL chorus register")
    s=unique_replace(s,
        """    const int reverb = std::clamp(static_cast<int>(vp.reverbSend) + chReverb, 0, 255);""",
        """    const int reverb = m_guiSendManual[0]
        ? std::clamp(int(std::lround(m_guiControls[10]*255.f)),0,255)
        : std::clamp(static_cast<int>(vp.reverbSend)+chReverb,0,255);""",
        "Synth.cpp ORIGINAL reverb register")
    hdr.write_text(h,encoding="utf-8")
    src.write_text(s,encoding="utf-8")
    print("P165_ORIGINAL_P175_NATIVE_GENERATOR_OVERLAYS_PATCHED_12_OF_12")
    print("P165_ZERO_CONTROLS_PRESERVE_PINNED_86BOX_CHIP_AND_CREATIVE_REGISTER_PATH")

if __name__=="__main__":
    main()
