#include "OriginalCreativeVolumeController.hpp"
#include <cstdint>

namespace awe64::original_vxd {
std::optional<std::uint8_t> original_creative_voice_attenuation(
    OriginalVolumeResources const& r,std::uint8_t velocity_level,
    std::int16_t amplitude_word,std::uint8_t cc7,
    std::uint8_t cc11,bool original_rom_sample) noexcept {
    if(!r.verified || !r.cc7_table_128 || !r.cc11_table_128 ||
       !r.device_additive_verified || !r.format1_verified ||
       cc7>=128u || cc11>=128u)return std::nullopt;
    // Original LCOD+0x640A stores CC7 in channel+0x08 and forwards the
    // current CC11 (channel+0x09) to +0x6439. Therefore +0x648E tests
    // CC11 <= 10 for immediate attenuation 0xff, NOT CC7 <= 10.
    // The +0x64BF table lookup is indexed by CC11. The later +0x64E7
    // lookup is indexed by CC7, and applies a separate scaling step.
    // Original signed x86 division is truncated toward zero.
    if(cc11<=10u)return 255u;
    const auto amplitude=(static_cast<std::int32_t>(amplitude_word)+12)/24;
    const auto weighted=static_cast<std::int32_t>(velocity_level)+
        static_cast<std::int32_t>(r.cc11_table_128[cc11])+amplitude;
    std::int32_t value=weighted*8/3+static_cast<std::int32_t>(r.device_additive);
    if(value>=255)return 255u;
    if(value<0)value=0;
    if(cc7<127u) {
        value+=((256-value)*static_cast<std::int32_t>(r.cc7_table_128[cc7]))/128;
    }
    if(r.format1 && original_rom_sample)value+=16;
    if(value>255)value=255;
    return static_cast<std::uint8_t>(value);
}
std::optional<std::uint8_t> original_note_attenuation(
    OriginalNoteVolumeResources const& r,std::uint8_t velocity,
    std::int16_t amplitude_word,std::uint8_t cc7,
    std::uint8_t cc11,bool original_rom_sample) noexcept {
    if(!r.original_velocity_table_verified || r.velocity_table.size()!=128u ||
       velocity==0u || velocity>=128u)return std::nullopt;
    return original_creative_voice_attenuation(r.controllers,
        r.velocity_table[velocity],amplitude_word,cc7,cc11,original_rom_sample);
}
MidiResult original_cc7_cc11_live(MidiEngine& e,
    const std::uint8_t ch,const std::uint8_t cc,
    const std::uint8_t value,const PortIo io) noexcept {
    if(e.fault)return MidiResult::FaultLatched;
    if(ch>=16u || (cc!=7u && cc!=11u) || value>127u ||
       e.device.physical_capacity==0u || e.device.physical_capacity>32u)
        return MidiResult::Invalid;
    auto& state=e.channel[ch];
    const auto& resource=e.volume_source;
    if(!resource.verified || !resource.cc7_table_128 ||
       !resource.cc11_table_128 ||
       !resource.device_additive_verified || !resource.format1_verified) {
        // MIDI was received, but the authentic original tables are not
        // available. Never emit a speculative IFATN write.
        if(cc==7u)state.volume=value;
        else state.expression=value;
        return MidiResult::Unsupported;
    }
    const auto new_cc7=cc==7u?value:state.volume;
    const auto new_cc11=cc==11u?value:state.expression;
    if(new_cc7>=128u || new_cc11>=128u)return MidiResult::Invalid;
    unsigned matched=0u;
    for(unsigned i=0u;i<e.device.physical_capacity;++i) {
        const auto& v=e.device.voices[i];
        if((v.key_marker>>8u)!=ch)continue;
        if(!v.volume_terms_verified)return MidiResult::MissingSource;
        ++matched;
    }
    if(matched!=0u && (!io.out8 || !io.in16 || !io.out16))
        return MidiResult::MissingSource;
    if(cc==7u)state.volume=value;
    else state.expression=value;
    for(unsigned i=0u;i<e.device.physical_capacity;++i) {
        const auto& voice=e.device.voices[i];
        if((voice.key_marker>>8u)!=ch)continue;
        const auto attenuation=original_creative_voice_attenuation(
            resource,voice.original_velocity_volume_level,
            voice.original_amplitude_word,new_cc7,new_cc11,voice.blocked);
        // All resource/channel gates were checked before the first write.
        if(!attenuation)return MidiResult::MissingSource;
        const auto selector=static_cast<std::uint8_t>(0x20u|i);
        std::uint16_t current=0;
        if(!io.out8(io.opaque,0xe22u,selector) ||
           !io.in16(io.opaque,0xe20u,current) ||
           !io.out8(io.opaque,0xe22u,selector) ||
           !io.out16(io.opaque,0xe20u,
                     static_cast<std::uint16_t>((current&0xff00u)|*attenuation))) {
            e.fault=true;
            return MidiResult::PortError;
        }
    }
    return MidiResult::Applied;
}
} // namespace awe64::original_vxd
