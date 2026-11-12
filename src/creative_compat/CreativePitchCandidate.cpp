#include "CreativePitchCandidate.h"
#include <cstdint>
#include <algorithm>
#include <initializer_list>
namespace awe64::creative {
namespace {
// Independent transcription of the arithmetic *meaning* of the author's
// original Creative Windows 95 SBAWE.VXD code, not copied original code.
// Private SHA256 d343f242...; LE locked-code pitch helper near 0x53AA:
// add 16800 cents, split in 1200-cent octaves, encode 4096 steps per octave
// with an exact 3*remainder + floor(31*remainder/75) fractional term.
// The driver saturates the octave field to 15.
constexpr std::optional<std::uint16_t> originalDriverCentsToIp(
    const std::int64_t pitch_cents) noexcept {
    const auto absolute=pitch_cents+16800;
    if(absolute<0)return std::nullopt;
    const auto octave=std::min<std::int64_t>(15,absolute/1200);
    const auto remainder=absolute%1200;
    const auto fractional=3*remainder+(31*remainder)/75;
    return static_cast<std::uint16_t>((octave<<12)|fractional);
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
    // Original Win95 1MGM branch supplies this source-relative pitch in
    // cents to its hardware-IP encoder. Generalized generator-55 behavior
    // under other SF1 bank formats still requires original-driver traces.
    const auto cents=std::int64_t(midi_key)*100-*raw;
    const auto unbent=originalDriverCentsToIp(cents);
    if(!unbent)return std::nullopt;
    // The measured default bend range is +/-200 cents. Express the signed
    // 14-bit fraction in chip pitch steps, truncating fractional steps toward
    // zero; the original register measurements confirm the three endpoints.
    const auto bend=(std::int64_t(bend14)-8192)*
                    std::int64_t(bend_range_cents)*4096/(8192*1200);
    // The original Win95 voice writepath saturates its computed 16-bit
    // pitch word after per-voice and per-channel offsets; no wraparound.
    const auto summed=std::int64_t(*unbent)+bend;
    const auto ip=std::clamp<std::int64_t>(summed,0,65535);
    return CreativePitchResult{static_cast<std::uint16_t>(ip),
        static_cast<std::int32_t>(*unbent)-0xe000,
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
