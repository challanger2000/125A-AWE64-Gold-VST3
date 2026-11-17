#include "OriginalCreativePanController.hpp"
#include <cstdint>

namespace awe64::original_vxd {
namespace {
[[nodiscard]] std::uint8_t target_pan(std::uint8_t base,
    std::uint8_t channel) noexcept {
    // Original x86 +0x6935..+0x6958:
    //   EAX=0x17F - 2*(voice+0x12 + channel_pan)
    //   if EAX>=254 -> 255; if EAX<=1 -> 0; else EAX
    const std::int32_t v=383-2*(static_cast<std::int32_t>(base)+channel);
    return static_cast<std::uint8_t>(v>=254?255:v<=1?0:v);
}
[[nodiscard]] bool write_pan_step(VoiceRecord& voice,
    std::uint8_t physical,PortIo io) noexcept {
    // Original +0x6992 excludes non-musical/ineligible row markers.
    if(voice.key_marker>=0x2000u || voice.pan_current==voice.pan_target)
        return true;
    if(!io.out8 || !io.in32 || !io.out32)return false;
    const std::uint8_t next=voice.pan_current<voice.pan_target
        ?static_cast<std::uint8_t>(voice.pan_current+1u)
        :static_cast<std::uint8_t>(voice.pan_current-1u);
    // Original +0x69C0..+0x6A0D: read 32-bit DATA0 PSST, preserve its
    // low 24 bits, update high byte; then read DATA0 PTRX, preserve its
    // HIGH 24 bits, update its low byte to unsigned negated pan, or 0xFF.
    // Do NOT accidentally write CSL; selector 0x20 is PTRX, not CSL.
    std::uint32_t psst=0u,ptrx=0u;
    if(!io.out8(io.opaque,0xe22u,static_cast<std::uint8_t>(0xc0u|physical)) ||
       !io.in32(io.opaque,0x620u,psst) ||
       !io.out8(io.opaque,0xe22u,static_cast<std::uint8_t>(0xc0u|physical)) ||
       !io.out32(io.opaque,0x620u,(psst&0x00ffffffu)|
                            (static_cast<std::uint32_t>(next)<<24u)))
        return false;
    const auto negative=next==0u?0xffu:static_cast<std::uint8_t>(0u-next);
    if(!io.out8(io.opaque,0xe22u,static_cast<std::uint8_t>(0x20u|physical)) ||
       !io.in32(io.opaque,0x620u,ptrx) ||
       !io.out8(io.opaque,0xe22u,static_cast<std::uint8_t>(0x20u|physical)) ||
       !io.out32(io.opaque,0x620u,(ptrx&0xffffff00u)|negative))
        return false;
    voice.pan_current=next;
    return true;
}
}
MidiResult original_channel_pan_cc10(MidiEngine& engine,
    const std::uint8_t channel,const std::uint8_t value,PortIo io) noexcept {
    if(engine.fault)return MidiResult::FaultLatched;
    if(channel>=16u || value>127u ||
       engine.device.physical_capacity==0u ||
       engine.device.physical_capacity>physical_voices)
        return MidiResult::Invalid;
    auto& ch=engine.channel[channel];
    if(!engine.device.original_pan_device_mode_verified) {
        ch.pan=value;
        return MidiResult::Unsupported;
    }
    if(ch.pan==value)return MidiResult::Applied;
    // The original skips voice writes for device+0x12 == 1.
    if(engine.device.original_pan_device_mode==1u) {
        ch.pan=value;
        return MidiResult::Applied;
    }
    bool any_pending=false;
    for(unsigned i=0u;i<engine.device.physical_capacity;++i) {
        const auto& v=engine.device.voices[i];
        if((v.key_marker>>8u)!=channel)continue;
        if(!v.pan_state_verified)return MidiResult::MissingSource;
        const auto wanted=target_pan(v.original_pan_base,value);
        if(v.pan_current!=wanted) {
            if(!io.out8 || !io.in32 || !io.out32)
                return MidiResult::MissingSource;
            any_pending=true;
        }
    }
    ch.pan=value;
    for(unsigned i=0u;i<engine.device.physical_capacity;++i) {
        auto& voice=engine.device.voices[i];
        if((voice.key_marker>>8u)!=channel)continue;
        voice.pan_target=target_pan(voice.original_pan_base,value);
        if(!write_pan_step(voice,static_cast<std::uint8_t>(i),io)) {
            engine.fault=true;
            return MidiResult::PortError;
        }
    }
    (void)any_pending;
    for(unsigned i=0u;i<engine.device.physical_capacity;++i) {
        const auto& voice=engine.device.voices[i];
        if((voice.key_marker>>8u)==channel &&
           voice.pan_current!=voice.pan_target)
            return MidiResult::Incomplete;
    }
    return MidiResult::Applied;
}
MidiResult service_original_pan_voice_step(MidiEngine& engine,
    const std::uint8_t physical,PortIo io) noexcept {
    if(engine.fault)return MidiResult::FaultLatched;
    if(physical>=engine.device.physical_capacity ||
       engine.device.physical_capacity>physical_voices)
        return MidiResult::Invalid;
    auto& row=engine.device.voices[physical];
    if(!row.pan_state_verified)return MidiResult::MissingSource;
    if(!write_pan_step(row,physical,io)) {
        engine.fault=true;
        return MidiResult::PortError;
    }
    return row.pan_current==row.pan_target?
        MidiResult::Applied:MidiResult::Incomplete;
}
} // namespace awe64::original_vxd
