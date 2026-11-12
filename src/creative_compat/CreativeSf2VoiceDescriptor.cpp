#include "CreativeSf2VoiceDescriptor.h"
#include "CreativeVxdVoiceEnvelopePrelude.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>
namespace awe64::creative {
namespace {
std::uint16_t u16(const CreativeSf2VoiceDescriptorBytes& b,
                  unsigned off) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<unsigned>(b[off])|(static_cast<unsigned>(b[off+1])<<8));
}
std::int16_t signedWord(const CreativeSf2VoiceDescriptorBytes& b,
                        unsigned op) noexcept {
    return static_cast<std::int16_t>(u16(b,op*2u));
}
void word(CreativeSf2VoiceDescriptorBytes& b,
          unsigned op,std::uint16_t value) noexcept {
    b[op*2u]=static_cast<std::uint8_t>(value);
    b[op*2u+1u]=static_cast<std::uint8_t>(value>>8);
}
std::uint16_t boundedWord(std::int32_t x,
                          std::int32_t low,std::int32_t high) noexcept {
    return static_cast<std::uint16_t>(std::clamp(x,low,high));
}
}
std::optional<CreativeSf2VoiceDescriptorBytes>
candidateOriginalSf2EncodedVoiceDescriptor(
    const CreativeSf2VoiceDescriptorBytes& merged,
    const OriginalSf2VoiceStartConditions& context) noexcept {
    if(context.midi_key>127u || context.midi_velocity==0u ||
       context.midi_velocity>127u || context.channel_pan>127u ||
       context.channel_chorus>127u || context.channel_reverb>127u)
        return std::nullopt;
    // This is the verified 1MGM ROM-mono path only. Linked stereo and
    // RAM samples must never silently use ROM addresses/voice settings.
    if(u16(merged,0x8cu)!=0x8001u)return std::nullopt;
    auto result=merged;
    // Original format-2 voice transition clears the ROM qualifier bit
    // in the POST-START descriptor: 0x8001 -> 0x0001. Its actual ROM
    // address was resolved earlier by the unchanged original ROM planner.
    word(result,0x8cu/2u,0x0001u);
    // Reuse the established original CC7/CC11/velocity/ROM attenuation.
    // Descriptor +0x60 is signed generator48; device state +0x10 is
    // independently supplied, never inferred from the SoundFont.
    auto volume=context.volume;
    volume.encoded_voice_amplitude=signedWord(merged,48u);
    volume.original_1mgm_rom_sample=true;
    const auto attenuation=candidateOriginalCreativeAttenuation(
        context.midi_velocity,volume);
    if(!attenuation)return std::nullopt;
    word(result,19u,*attenuation);
    const auto attack=originalCreativeSf2VoiceAttackRate(
        signedWord(merged,34u));
    // Filter gate tests DESCRIPTOR attack <125. The independent later
    // hardware port writer maps exact attack128 to byte127, not here.
    unsigned cutoff=boundedWord(signedWord(merged,8u),0,255);
    if(context.creative_bank<128u && attack<125u)
        cutoff=(std::max(70u,static_cast<unsigned>(context.midi_velocity))*
                   cutoff+160u)>>7u;
    word(result,8u,static_cast<std::uint16_t>(cutoff));
    word(result,9u,boundedWord(signedWord(merged,9u),0,15));
    for(const unsigned op:{5u,6u,7u,10u,11u,13u})
        word(result,op,boundedWord(signedWord(merged,op),-128,127));
    // Four key-to-hold/decay pairs are applied BEFORE PCOD+0x3BD4.
    // Operands must come from the original unmodified merged descriptor.
    constexpr std::array<std::pair<unsigned,unsigned>,4> scaled{{
        {27u,31u},{28u,32u},{35u,39u},{36u,40u}}};
    std::array<std::int16_t,59> inputs{};
    for(unsigned op=0;op<59;++op)inputs[op]=signedWord(merged,op);
    for(const auto& [target,scaler]:scaled) {
        const auto value=originalCreativeSf2KeyScaledEnvelopeWord(
            static_cast<std::uint16_t>(target),inputs[target],
            inputs[scaler],context.midi_key,context.creative_bank);
        if(!value)return std::nullopt;
        inputs[target]=*value;
    }
    // Observed original 1996 PCOD+0x3BD4 call order.
    constexpr std::array<unsigned,14> conversions{{
        26u,34u,21u,23u,25u,33u,28u,36u,30u,38u,
        27u,35u,22u,24u}};
    for(unsigned op:conversions) {
        const auto encoded=originalCreativeSf2VoiceEnvelopeOrLfoWord(
            static_cast<std::uint16_t>(op),inputs[op]);
        if(!encoded)return std::nullopt;
        word(result,op,*encoded);
    }
    for(unsigned op:{29u,37u})
        word(result,op,boundedWord(inputs[op],0,127));
    // Original SF2 bank pan -> record pan -> common channel pan packer.
    // Multiply rather than signed-shift to avoid negative-shift UB.
    const auto recordPan=std::clamp<std::int32_t>(
        (static_cast<std::int32_t>(signedWord(merged,17u))*16)/125+64,
        0,127);
    const auto panCoord=383-2*(static_cast<std::int32_t>(
        context.channel_pan)+recordPan);
    const auto pan=static_cast<std::uint8_t>(
        panCoord>=254?255:panCoord<2?0:panCoord);
    word(result,17u,pan);
    word(result,18u,pan==0u?255u:static_cast<std::uint8_t>(-pan));
    // Zero Device effects flag scales the bank sends *100/392 before
    // adding channel send bytes, then saturating [0,255].
    for(const auto& [op,channel]:
        std::array<std::pair<unsigned,std::uint8_t>,2>{{
            {15u,context.channel_chorus},{16u,context.channel_reverb}}}) {
        std::int32_t send=signedWord(merged,op);
        if(context.device_effects_flag==0u)send=(send*100)/392;
        word(result,op,boundedWord(send+channel,0,255));
    }
    return result;
}
} // namespace awe64::creative
