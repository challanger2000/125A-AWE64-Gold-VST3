#pragma once
// P007AI: ORIGINAL Creative AWE32/64 Part IV NRPN LSB 24.
// This is an OWNER-PRIVATE QA-only signed modulation-depth control, NOT
// production DSP or an automatic alteration of the user's SoundFont preset.
// Creative original specification: NRPN24 [-127,+127], 56.25 cents per
// unit, up to six octaves envelope1-to-filter-frequency modulation.
#include <array>
#include <cstdint>
#include <algorithm>
namespace p007::original_filter_env_depth {
struct MidiCC {std::uint8_t controller=0u,value=0u;};
inline constexpr std::uint8_t kNrpnMsb=127u;
inline constexpr std::uint8_t kModEnvToFilterCutoff=24u;
inline std::array<MidiCC,4> originalCreativeNrpn24(int signedDepth) noexcept {
    const unsigned word=static_cast<unsigned>(
        std::clamp(signedDepth,-127,127)+8192);
    return {{{99u,127u},{98u,kModEnvToFilterCutoff},
             {6u,static_cast<std::uint8_t>((word>>7u)&127u)},
             {38u,static_cast<std::uint8_t>(word&127u)}}};
}
inline int decodeOriginalCreativeSignedValue(const std::array<MidiCC,4>& cc)
                                                            noexcept {
    if(cc[0].controller!=99u||cc[0].value!=127u||
       cc[1].controller!=98u||cc[1].value!=24u||
       cc[2].controller!=6u||cc[3].controller!=38u)
       return -10000; // malformed / not original Creative NRPN24
    return static_cast<int>((unsigned(cc[2].value)<<7u)|
                              unsigned(cc[3].value))-8192;
}
class Transmitter final {
    std::array<MidiCC,4> data_{};
    unsigned stage_=0u;
    bool active_=false;
public:
    void arm(int depth) noexcept {
        data_=originalCreativeNrpn24(depth);stage_=0u;active_=true;
    }
    bool active() const noexcept {return active_;}
    unsigned stage() const noexcept {return stage_;}
    template<class Sender>
    bool tick(Sender&& send) noexcept {
        if(!active_)return false;
        const auto cc=data_[stage_];
        if(!send(cc.controller,cc.value))
            return true; // FIFO busy: retry the same 6-byte MIDI packet.
        if(++stage_==4u){stage_=0u;active_=false;}
        return true;
    }
};
} // namespace p007::original_filter_env_depth
