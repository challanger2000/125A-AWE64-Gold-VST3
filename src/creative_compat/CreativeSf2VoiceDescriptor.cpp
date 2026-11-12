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
std::optional<std::uint16_t> candidateOriginalSf2InitialIpFromDescriptor(
    const CreativeSf2VoiceDescriptorBytes& descriptor,
    const std::uint8_t played_key,
    const std::int16_t channel_tune_1,
    const std::int16_t channel_tune_2) noexcept {
    if(played_key>127u || descriptor[0x8au]>127u)
        return std::nullopt;
    const auto rawRoot=static_cast<std::int32_t>(signedWord(descriptor,58u));
    const auto root=rawRoot==-1
        ? static_cast<std::int32_t>(descriptor[0x8au]):rawRoot;
    if(root<0 || root>127)return std::nullopt;
    // The source stores dwPitch as an unsigned DWORD at +0x86.
    const std::uint32_t dwPitch=
        static_cast<std::uint32_t>(descriptor[0x86u]) |
        (static_cast<std::uint32_t>(descriptor[0x87u])<<8u) |
        (static_cast<std::uint32_t>(descriptor[0x88u])<<16u) |
        (static_cast<std::uint32_t>(descriptor[0x89u])<<24u);
    if(dwPitch>0xffffffu)return std::nullopt;
    const auto correctionByte=static_cast<std::int32_t>(descriptor[0x8bu]);
    const auto correction=correctionByte<128?correctionByte:correctionByte-256;
    const auto scale=static_cast<std::int64_t>(signedWord(descriptor,56u));
    // Original PCOD note-start: all inputs are integer cents. CoarseTune
    // (gen51) has already been multiplied by 100 at the format-2 bank
    // conversion stage; do NOT scale it twice. Emit exact source 874
    // integer steps/cent, not a floating point pitch approximation.
    const auto offsetCents=static_cast<std::int64_t>(correction)+
        signedWord(descriptor,52u)+
        static_cast<std::int64_t>(channel_tune_1)+channel_tune_2+
        signedWord(descriptor,51u)+
        (static_cast<std::int64_t>(played_key)-root)*scale;
    const auto pitch=static_cast<std::int64_t>(dwPitch)+offsetCents*874ll;
    // Source confirms upper 24-bit saturation. Negative pitch behavior
    // has not been independently characterized: reject instead of wrap.
    if(pitch<0)return std::nullopt;
    return static_cast<std::uint16_t>(
        static_cast<std::uint64_t>(std::min<std::int64_t>(pitch,0xffffffll))>>8u);
}
std::optional<std::array<PortWord,2>>
candidateOriginalSf2InitialIpWritesFromDescriptor(
    const CreativeSf2VoiceDescriptorBytes& descriptor,
    const std::uint8_t played_key,const std::uint8_t physical_voice,
    const std::int16_t channel_tune_1,
    const std::int16_t channel_tune_2) noexcept {
    // Original AWE32 uses 30 musical voices; 30/31 are special/DRAM
    // refresh voices. Never silently emit an IP write to those lanes.
    if(physical_voice>=30u)return std::nullopt;
    const auto ip=candidateOriginalSf2InitialIpFromDescriptor(
        descriptor,played_key,channel_tune_1,channel_tune_2);
    if(!ip)return std::nullopt;
    return std::array<PortWord,2>{{
        PortWord{0xE22u,physical_voice},
        PortWord{0xE20u,*ip}
    }};
}
std::optional<OriginalCreativeSf2PitchLookupCoordinates>
originalCreativeSf2PitchLookupCoordinates(
    const std::uint32_t sample_rate_hz) noexcept {
    if(sample_rate_hz==0u)return std::nullopt;
    // Original VXD PCOD+0x3735 uses the low DWORD of this multiply.
    // Explicit uint64 arithmetic then truncation avoids host signed UB.
    const auto wrapped=static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(sample_rate_hz)*0x2f8eull);
    if(wrapped==0u)return std::nullopt; // original normalization unbounded
    auto normalized=wrapped;
    std::uint8_t shifts=0;
    while((normalized&0x80000000u)==0u){
        normalized<<=1u;
        ++shifts;
    }
    return OriginalCreativeSf2PitchLookupCoordinates{
        wrapped,normalized,shifts,
        static_cast<std::uint8_t>((normalized>>24u)&0x7fu),
        static_cast<std::uint8_t>((normalized>>17u)&0x7fu)};
}
std::optional<CreativeSf2VoiceDescriptorBytes>
candidateOriginalSf2GeneratorsWithVerifiedTemplates(
    const SbkLayer& layer,
    const std::array<std::int16_t,59>& original_preset_template,
    const std::array<std::int16_t,59>& original_instrument_template) noexcept {
    if(layer.source_format!=OriginalCreativeBankFormat::Format2Sf2 ||
       layer.key>127u || layer.velocity==0u || layer.velocity>127u ||
       layer.program>127u || layer.instrument_local.get(53u).has_value() &&
       *layer.instrument_local.get(53u)!=layer.rom_sample_selector)
        return std::nullopt;
    // The original Format-2 PCOD load stage is NOT the format-1 1MGM
    // overwrite rule. Each level starts with its own original template,
    // and within that level the latest local generator overrides global.
    // Every original operator 0..58 has an established PCOD load converter.
    CreativeSf2VoiceDescriptorBytes out{};
    for(unsigned op=0;op<59u;++op) {
        // Actual original-driver differential: instrument keyRange local
        // values 0x423A/0x4600/0x6B00/0x7300 select matching zones, but
        // the built 148-byte descriptor's generator43 remains 0x7F00.
        // keyRange/velRange are SELECTION predicates, not merged sound
        // parameters. Preserve the separately verified template values.
        if(op==43u || op==44u) {
            const auto sum=static_cast<std::int32_t>(original_preset_template[op])+
                           static_cast<std::int32_t>(original_instrument_template[op]);
            word(out,op,static_cast<std::uint16_t>(
                std::clamp(sum,-32768,32767)));
            continue;
        }
        const auto rawPreset=layer.preset_local.get(op).has_value()
            ? layer.preset_local.get(op):layer.preset_global.get(op);
        const auto rawInstrument=layer.instrument_local.get(op).has_value()
            ? layer.instrument_local.get(op):layer.instrument_global.get(op);
        const auto preset=rawPreset
            ? candidateOriginalSf2PresetDeltaGenerator(
                static_cast<std::uint16_t>(op),
                static_cast<std::int16_t>(*rawPreset))
            : std::optional<std::int16_t>(original_preset_template[op]);
        const auto inst=rawInstrument
            ? candidateOriginalSf2InstrumentLoadGenerator(
                static_cast<std::uint16_t>(op),
                static_cast<std::int16_t>(*rawInstrument))
            : std::optional<std::int16_t>(original_instrument_template[op]);
        if(!preset || !inst)return std::nullopt;
        // Original driver zeros the PRESET keynum/velocity before the
        // 59-WORD signed merge; the actual NoteOn key/velocity are known
        // only after zone selection. Never add them as two WORD deltas.
        const auto p=(op==46u || op==47u)?0:static_cast<std::int32_t>(*preset);
        const auto sum=static_cast<std::int32_t>(*inst)+p;
        word(out,op,static_cast<std::uint16_t>(
            std::clamp(sum,-32768,32767)));
    }
    // The driver resolves structural linkage through the selected zone,
    // not by signed-adding unrelated instrument and sample indices.
    // The key/velocity fields in this note's descriptor reflect its
    // actual MIDI selection, not the two unresolved template words.
    word(out,46u,layer.key);
    word(out,47u,layer.velocity);
    word(out,53u,layer.rom_sample_selector);
    return out;
}
std::optional<CreativeSf2VoiceDescriptorBytes>
candidateOriginalSf2VoiceWithVerifiedRomSampleMetadata(
    const CreativeSf2VoiceDescriptorBytes& merged,
    const SbkRomAddress& sample,
    const std::optional<std::uint32_t> verified_pitch,
    const std::uint32_t original_preset_group) noexcept {
    if(sample.source_format!=OriginalCreativeBankFormat::Format2Sf2 ||
       sample.sample_type!=0x8001u || sample.sample_rate_hz==0u ||
       sample.original_pitch>127u || !verified_pitch ||
       sample.start_word>=sample.end_word ||
       sample.start_word>sample.loop_start_word ||
       sample.loop_start_word>=sample.loop_end_word ||
       sample.loop_end_word>sample.end_word)
        return std::nullopt;
    auto result=merged;
    const auto dword=[&](const unsigned off,const std::uint32_t value) noexcept {
        for(unsigned i=0;i<4u;++i)
            result[off+i]=static_cast<std::uint8_t>(value>>(8u*i));
    };
    dword(0x76u,sample.start_word);
    dword(0x7au,sample.end_word);
    dword(0x7eu,sample.loop_start_word);
    dword(0x82u,sample.loop_end_word);
    dword(0x86u,*verified_pitch);
    result[0x8au]=sample.original_pitch;
    result[0x8bu]=static_cast<std::uint8_t>(sample.pitch_correction);
    word(result,0x8cu/2u,sample.sample_type);
    word(result,0x8eu/2u,sample.sample_link);
    dword(0x90u,original_preset_group);
    return result;
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
