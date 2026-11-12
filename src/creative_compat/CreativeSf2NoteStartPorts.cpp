#include "CreativeSf2NoteStartPorts.h"
#include <algorithm>
#include <array>
#include <cstdint>
namespace awe64::creative {
namespace {
std::uint16_t readWord(const CreativeSf2VoiceDescriptorBytes& b,
                       const unsigned generator) noexcept {
    return static_cast<std::uint16_t>(static_cast<unsigned>(b[2u*generator])|
           (static_cast<unsigned>(b[2u*generator+1u])<<8u));
}
} // namespace

std::optional<
    std::array<OriginalWin95VxdPortWrite,kOriginalSf2NoteStartPrefixPortCount>>
candidateOriginalSf2NoteStartRegisterPrefix(
    const CreativeSf2VoiceDescriptorBytes& note,
    const std::uint8_t key,const std::uint8_t voice,
    const bool from_verified_original_driver,
    const std::int16_t channel_tune_1,
    const std::int16_t channel_tune_2) noexcept {
    if(!from_verified_original_driver || key>127u ||
       voice>=kOriginalCreativeMusicVoices ||
       readWord(note,0x8cu/2u)!=0x0001u ||
       readWord(note,8u)>255u || readWord(note,19u)>255u ||
       readWord(note,34u)>128u || readWord(note,26u)>128u ||
       readWord(note,28u)>127u || readWord(note,29u)>127u)
        return std::nullopt;

    // The genuine original 148-byte AFTER NOTE-START record uses these
    // offsets, NOT the initial raw SF2 timecents:
    //  +0x44/+0x46 VOL ATKHLD low/high, +0x2A LFO1 delay,
    //  +0x34/+0x36 MOD ATKHLD low/high, +0x38/+0x3A DCYSUS,
    //  +0x2E LFO2 delay.
    OriginalVxdEncodedVoiceEnvelopePrelude encoded{};
    encoded.from_actual_original_driver_record=true;
    encoded.attack_hold_volume_low_byte_word=readWord(note,34u);
    encoded.attack_hold_volume_high_word=
        static_cast<std::int16_t>(readWord(note,35u));
    encoded.lfo1_delay_word=static_cast<std::int16_t>(readWord(note,21u));
    encoded.attack_hold_mod_low_byte_word=readWord(note,26u);
    encoded.attack_hold_mod_high_word=
        static_cast<std::int16_t>(readWord(note,27u));
    encoded.decay_sustain_mod_low_byte=
        static_cast<std::uint8_t>(readWord(note,28u));
    encoded.decay_sustain_mod_high_word=
        static_cast<std::int16_t>(readWord(note,29u));
    encoded.lfo2_delay_word=static_cast<std::int16_t>(readWord(note,23u));
    const auto envelope=candidateOriginalVxdVoiceEnvelopePrelude(
        voice,encoded);
    if(!envelope)return std::nullopt;
    const auto pitch=candidateOriginalSf2InitialIpWritesFromDescriptor(
        note,key,voice,channel_tune_1,channel_tune_2);
    if(!pitch)return std::nullopt;

    const auto ifatn=static_cast<std::uint16_t>(
        (readWord(note,8u)<<8u)|readWord(note,19u));
    std::array<OriginalWin95VxdPortWrite,
               kOriginalSf2NoteStartPrefixPortCount> result{};
    std::copy(envelope->begin(),envelope->end(),result.begin());
    // These four typed OUTs directly follow the 14-source-OUT prelude
    // (original x86 handoff trace indices 19..22, GM0/GM40/GM100).
    result[14]={0xe22u,(*pitch)[0].value,8u};
    result[15]={0xe20u,(*pitch)[1].value,16u};
    result[16]={0xe22u,static_cast<std::uint32_t>((1u<<5u)|voice),8u};
    result[17]={0xe20u,ifatn,16u};
    return result;
}
} // namespace awe64::creative
