#include "OriginalCreativeModulationController.hpp"
#include <cstdint>
namespace awe64::original_vxd {
MidiResult original_cc1_modulation_live(MidiEngine& e,
    const std::uint8_t ch,const std::uint8_t value,const PortIo io) noexcept {
    if(e.fault)return MidiResult::FaultLatched;
    if(ch>=16u || value>127u || e.device.physical_capacity==0u ||
       e.device.physical_capacity>physical_voices)return MidiResult::Invalid;
    auto& channel=e.channel[ch];
    const std::uint8_t bucket=static_cast<std::uint8_t>(value/30u);
    unsigned matched=0;
    for(unsigned i=0u;i<e.device.physical_capacity;++i) {
        const auto& v=e.device.voices[i];
        if((v.key_marker>>8u)!=ch)continue;
        if(!v.original_modulation_word_verified ||
           !channel.original_modulation_byte_44d_verified)
            return MidiResult::MissingSource;
        ++matched;
    }
    if(matched && (!io.out8 || !io.in16 || !io.out16))
        return MidiResult::MissingSource;
    channel.modulation=value;
    channel.original_modulation_bucket=bucket;
    for(unsigned i=0u;i<e.device.physical_capacity;++i) {
        const auto& voice=e.device.voices[i];
        if((voice.key_marker>>8u)!=ch)continue;
        const auto sum=static_cast<std::int32_t>(
            voice.original_modulation_word_signed)+
            channel.original_modulation_byte_44d+bucket;
        const auto clamped=sum>127?127:sum;
        // Original +0x6644 SHL DWORD 8: negative signed values retain
        // their LOW byte, rather than incorrectly saturating to zero.
        const auto high=static_cast<std::uint16_t>(
            static_cast<std::uint32_t>(clamped)<<8u);
        const auto selector=static_cast<std::uint8_t>(0x60u|i);
        std::uint16_t previous=0;
        if(!io.out8(io.opaque,0xe22u,selector) ||
           !io.in16(io.opaque,0xe20u,previous) ||
           !io.out8(io.opaque,0xe22u,selector) ||
           !io.out16(io.opaque,0xe20u,
                     static_cast<std::uint16_t>((previous&0x00ffu)|high))) {
            e.fault=true;return MidiResult::PortError;
        }
    }
    return MidiResult::Applied;
}
} // namespace awe64::original_vxd
