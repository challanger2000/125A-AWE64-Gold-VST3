#include "CreativePitchCandidate.h"
#include <cstdint>
namespace awe64::creative {
namespace {
constexpr std::int64_t floorQuotient(std::int64_t numerator,
                                     std::int64_t positive_denominator) noexcept {
    const auto quotient=numerator/positive_denominator;
    return quotient-(numerator<0 && numerator%positive_denominator!=0 ? 1 : 0);
}
std::optional<std::uint16_t> inherited(const SbkGeneratorScope& local,
                                       const SbkGeneratorScope& global,
                                       unsigned generator) noexcept {
    const auto selected=local.get(generator);
    return selected?selected:global.get(generator);
}
bool unsupportedTuning(const SbkLayer& layer) noexcept {
    // Avoid making up Creative interpretations for known SF2/SF1 tuning,
    // root key and fixed-key generators. Zero coarse/fine is neutral.
    const auto scopes={&layer.preset_global,&layer.preset_local,
                       &layer.instrument_global,&layer.instrument_local};
    for(const auto* p:scopes) {
        for(const auto op:{51u,52u,46u,58u}) {
            const auto v=p->get(op);
            if(v && *v!=0) return true;
        }
        const auto scale=p->get(56);
        if(scale && *scale!=100) return true;
    }
    return false;
}
}
std::optional<CreativePitchResult> candidateOriginalRomIp(
    const SbkLayer& layer,const std::uint8_t midi_key,
    const std::uint16_t bend14,const std::uint16_t bend_range_cents) noexcept {
    if(midi_key>127 || midi_key!=layer.key || bend14>16383 ||
       bend_range_cents>2400 || unsupportedTuning(layer))return std::nullopt;
    // For this authentic 1MGM bank gen55 was observed as an effective
    // pitch coordinate for GM0 and GM40. It is not a generic SF2 rule.
    const auto raw=inherited(layer.instrument_local,layer.instrument_global,55);
    if(!raw || *raw>12799 || layer.preset_global.get(55) ||
       layer.preset_local.get(55))return std::nullopt;
    // EMU8000 IP: E000 = unity; 100 cents = 1 semitone;
    // 4096 chip pitch steps per octave. Negative base shifts round DOWN.
    const auto base_numerator=(std::int64_t(midi_key)*100-*raw)*4096;
    const auto base=floorQuotient(base_numerator,1200);
    // In the measured Creative default pitch bend range (200 cents),
    // center is 8192; signed fractions truncate toward zero.
    const auto bend=(std::int64_t(bend14)-8192)*
                    std::int64_t(bend_range_cents)*4096/(8192*1200);
    const auto ip=0xe000+base+bend;
    if(ip<0 || ip>0xffff)return std::nullopt; // no arbitrary wrap or gain trick
    return CreativePitchResult{static_cast<std::uint16_t>(ip),
        static_cast<std::int32_t>(base),
        static_cast<std::int32_t>(bend),*raw};
}
std::optional<std::array<PortWord,2>> candidateOriginalRomIpWrites(
    const CreativeRomNotePlan& note,const std::size_t index,
    const std::uint8_t voice,const MidiState& state,
    const std::uint16_t bend_range_cents) noexcept {
    // The two high physical voice channels are normally reserved for DRAM
    // refresh on the AWE32. Don't silently allocate them as musical voices.
    if(index>=note.count || note.count>kMaxCreative1MgmLayers ||
       voice>=30 || note.channel>=16 || note.key>127 ||
       note.velocity==0 || note.velocity>127)return std::nullopt;
    const auto& layer=note.layers[index].metadata;
    if(layer.key!=note.key || layer.velocity!=note.velocity ||
       layer.program!=note.program || layer.bank!=note.original_sbk_bank)
        return std::nullopt;
    const auto pitch=candidateOriginalRomIp(layer,note.key,
        state.channel(note.channel).pitch_bend14,bend_range_cents);
    if(!pitch)return std::nullopt;
    return std::array<PortWord,2>{{
        PortWord{0xE22u,voice}, // DATA3 register 0 (IP), physical voice
        PortWord{0xE20u,pitch->ip}
    }};
}
} // namespace awe64::creative
