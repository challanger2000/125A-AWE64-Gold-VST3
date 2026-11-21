#pragma once
// P016: Original AWE32Emu / Creative SBAWE.VXD channel defaults.
// Shared by production VST3 project restore and public chip-audio QA.
// Not a DSP modification; do not call on a clean VST3 activation.
// Values are verbatim from pinned Synth::ChannelState initialization.
#include "Synth.h"
#include <cstdint>
namespace p009 {
inline void resetOriginalCreativeControllers(Synth& synth) noexcept {
    for(unsigned ch=0;ch<Synth::kChannelCount;++ch){
        const auto channel=static_cast<std::uint8_t>(ch);
        synth.ControlChange(channel,64,0);     // sustain off
        synth.ControlChange(channel,1,0);      // modulation
        synth.ControlChange(channel,7,100);    // volume
        synth.ControlChange(channel,10,64);    // pan center
        synth.ControlChange(channel,11,127);   // expression
        synth.ControlChange(channel,91,0);     // dry reverb send
        synth.ControlChange(channel,93,0);     // dry chorus send
        synth.ControlChange(channel,101,0);    // select RPN 0,0
        synth.ControlChange(channel,100,0);
        synth.ControlChange(channel,6,2);      // default pitch range
        synth.ControlChange(channel,101,127);  // deselect RPN
        synth.ControlChange(channel,100,127);
        synth.PitchBend(channel,0);            // center
    }
}
} // namespace p009
