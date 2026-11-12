#include "CreativeControllerVolume.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace awe64::creative {
namespace {
// Reconstructed functional lookup coordinates from the owner's installed
// Creative Windows 95 SBAWE.VXD, source SHA-256
// d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a.
// These are numerical conversion constants, not original code or driver bytes.
// Original CC7 table in LCOD object around +0x2EA0.
constexpr std::array<std::uint8_t,128> kControllerVolume{
  99,99,99,99,99,99,99,99,99,99,99,43,41,40,39,38,
  37,36,35,34,33,32,31,30,30,29,28,27,27,26,25,25,
  24,23,23,22,22,21,21,20,20,19,19,19,18,18,17,17,
  17,16,16,16,15,15,15,14,14,14,14,13,13,13,12,12,
  12,12,12,11,11,11,11,10,10,10,10,9,9,9,9,9,
  8,8,8,8,8,7,7,7,7,6,6,6,6,6,5,5,
  5,5,5,4,4,4,4,4,3,3,3,3,3,2,2,2,
  2,2,2,1,1,1,1,1,1,1,0,0,0,0,0,0
};
// Original CC11 expression lookup in LCOD around +0x05E8.
// Expression correction happens BEFORE the ROM +16 attenuation term.
constexpr std::array<std::uint8_t,128> kExpression{
  127,108,98,90,84,80,75,72,69,66,64,61,59,57,56,54,
  52,51,49,48,47,45,44,43,42,41,40,39,38,37,36,36,
  35,34,33,33,32,31,30,30,29,29,28,27,27,26,26,25,
  24,24,23,23,22,22,21,21,21,20,20,19,19,18,18,17,
  17,17,16,16,15,15,14,14,14,13,13,13,12,12,12,
  11,11,11,10,10,10,9,9,9,9,8,8,8,7,7,7,
  7,6,6,6,6,5,5,5,4,4,4,4,4,3,3,3,
  3,2,2,2,2,1,1,1,1,1,0,0,0,0,0,0,0
};
static_assert(kControllerVolume.size()==128 && kExpression.size()==128);
static_assert(kControllerVolume[20]==33 && kControllerVolume[100]==4 &&
              kControllerVolume[127]==0 && kExpression[30]==36);
}
std::optional<std::uint8_t> candidateOriginalCreativeAttenuation(
    const std::uint8_t velocity, const OriginalCreativeVolumeTerms& p) noexcept {
    if(velocity==0 || velocity>127 || p.cc7>127 || p.cc11>127 ||
       p.encoded_voice_amplitude<0 || p.encoded_sample_attenuation>255)
        return std::nullopt;
    // Original VXD branch hard-mutes CC7=0..10 before any ROM correction.
    if(p.cc7<=10)return std::uint8_t{255};
    // The original signed amplitude operand is added as (word+12)/24.
    const auto ampBucket=static_cast<unsigned>(
        (static_cast<int>(p.encoded_voice_amplitude)+12)/24);
    const auto velocityBucket=static_cast<unsigned>(
        originalCreativeMeasuredVelocityLevel(velocity));
    const unsigned sum=static_cast<unsigned>(kControllerVolume[p.cc7])
        +velocityBucket+ampBucket;
    unsigned volume=std::min(255u,(sum*8u)/3u+
                                  static_cast<unsigned>(p.encoded_sample_attenuation));
    if(p.cc11<127)
        volume+=((256u-volume)*static_cast<unsigned>(kExpression[p.cc11]))/128u;
    if(p.original_1mgm_rom_sample)volume+=16u;
    return static_cast<std::uint8_t>(std::min(volume,255u));
}
std::optional<InitialIfatn> candidateOriginalCreativeIfatn(
    std::uint8_t velocity, const OriginalCreativeIfatnTerms& terms) noexcept {
    const auto vol=candidateOriginalCreativeAttenuation(velocity,terms.volume);
    if(!vol)return std::nullopt;
    // The encoded attack gate was recovered from the same original VXD.
    // The legacy IFATN function's attenuation estimate is discarded here:
    // exact driver-derived CC7/CC11/ROM order above owns the low byte.
    const auto filter=candidateOriginalIfatnFromEncodedAttack(
        velocity,CandidateVoiceParameters{0,0,terms.initial_cutoff,false},
        terms.bank,terms.encoded_volume_attack_rate);
    if(!filter)return std::nullopt;
    const auto cut=filter->cutoff;
    return InitialIfatn{cut,*vol,static_cast<std::uint16_t>(
        (static_cast<unsigned>(cut)<<8)|*vol)};
}
std::optional<std::array<PortWord,2>> candidateOriginalCreativeIfatnWrites(
    const MidiMessage& midi,const std::uint8_t voice,const MidiState& state,
    const OriginalCreativeIfatnTerms& instrument) noexcept {
    if(midi.kind!=MidiKind::NoteOn || midi.data2==0 || midi.data2>127 ||
       midi.channel>=16 || midi.data1>127 || voice>=30)
        return std::nullopt;
    auto params=instrument;
    const auto& ch=state.channel(midi.channel);
    params.volume.cc7=ch.cc7_volume;
    params.volume.cc11=ch.cc11_expression;
    const auto result=candidateOriginalCreativeIfatn(midi.data2,params);
    if(!result)return std::nullopt;
    return std::array<PortWord,2>{{
        PortWord{0xE22u,static_cast<std::uint16_t>(0x20u|voice)},
        PortWord{0xE20u,result->word}
    }};
}

std::optional<std::array<PortWord,2>> candidateOriginalCreativeLiveVolumeWrites(
    const MidiMessage& event,const MidiState& state,
    const ActiveOriginalCreativeVoice& v) noexcept {
    if(event.kind!=MidiKind::Controller ||
       (event.data1!=7 && event.data1!=11) ||
       event.channel>=16 || event.channel!=v.channel ||
       event.data2>127 || !v.sounding ||
       v.midi_key>127 || v.note_on_velocity==0 ||
       v.note_on_velocity>127 || v.physical_voice>=30)
        return std::nullopt;
    const auto& channel=state.channel(event.channel);
    // A caller that forgot to apply the event first must not produce stale
    // register traffic. Another channel must not update this voice.
    if((event.data1==7 && channel.cc7_volume!=event.data2) ||
       (event.data1==11 && channel.cc11_expression!=event.data2))
        return std::nullopt;
    auto terms=v.instrument;
    terms.volume.cc7=channel.cc7_volume;
    terms.volume.cc11=channel.cc11_expression;
    const auto current=candidateOriginalCreativeIfatn(v.note_on_velocity,terms);
    if(!current)return std::nullopt;
    return std::array<PortWord,2>{{
        PortWord{0xE22u,static_cast<std::uint16_t>(0x20u|v.physical_voice)},
        PortWord{0xE20u,current->word}
    }};
}
} // namespace awe64::creative
