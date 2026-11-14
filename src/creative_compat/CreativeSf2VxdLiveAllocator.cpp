#include "CreativeSf2VxdLiveAllocator.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
namespace awe64::creative {
namespace {
std::uint16_t le16(const std::span<const std::uint8_t> b,
                   const std::size_t at) noexcept {
    return static_cast<std::uint16_t>(b[at] |
        (static_cast<unsigned>(b[at+1u])<<8u));
}
std::uint32_t le32(const std::span<const std::uint8_t> b,
                   const std::size_t at) noexcept {
    return static_cast<std::uint32_t>(le16(b,at)) |
           (static_cast<std::uint32_t>(le16(b,at+2u))<<16u);
}
std::uint32_t readHardware32(awe64::Chip& chip,
                             const std::uint8_t register_index,
                             const std::uint8_t voice,
                             const std::uint16_t port) noexcept {
    chip.writePort(0xe22u,static_cast<std::uint16_t>(
        (static_cast<unsigned>(register_index)<<5u)|voice));
    return static_cast<std::uint32_t>(chip.readPort(port)) |
           (static_cast<std::uint32_t>(chip.readPort(
                static_cast<std::uint16_t>(port+2u)))<<16u);
}
}
std::optional<OriginalWin95MonoSelection>
candidateOriginalSf2LiveVxdMonoVoice(
    awe64::Chip& chip,
    const std::span<const std::uint8_t> state,
    const bool verified) noexcept {
    if(!verified || !chip.ready() ||
       state.size()<kOriginalVxdMinDeviceImage ||
       le32(state,kOriginalVxdRomIdOffset)!=0x4d474d31u ||
       le16(state,0x12u)!=0u)
        return std::nullopt;
    // VERIFIED from the real 2026-11-14 original Win95 SBAWE.VXD
    // resident memory: device+0x06 = 32 and device+0x18 = 32 are
    // the PHYSICAL EMU8000 capacity, NOT the 30-slot music budget.
    // The unchanged original LCOD music allocator scans rows 0..29;
    // physical voices 30 and 31 remain reserved. Previous code
    // wrongly rejected authentic 32-voice VXD state as >30.
    const auto capacity=le16(state,0x06u);
    const auto mode_count=le32(state,0x18u);
    if(capacity!=32u || mode_count!=32u)return std::nullopt;
    constexpr auto usable=kOriginalCreativeMusicVoices;
    std::array<OriginalWin95MonoVoiceSnapshot,
               kOriginalCreativeMusicVoices> voices{};
    // SOURCE VXD LCOD+0x3EA1 three interleaved lanes; reads from
    // VTFT (register3 / DATA0), DCYSUSV (register5 / DATA1),
    // and current CCCA (register0 / DATA1) using the pinned chip,
    // rather than a modern envelope approximation.
    //
    // This bounded diagnostic samples ALL allowed voices once.
    // The exact original early-exit READ timestamps and side effects
    // require a separate full Win95 trace; not claimed here.
    for(std::size_t i=0;i<usable;++i) {
        const auto at=kOriginalVxdVoiceRowStart+
                      i*kOriginalVxdVoiceRowStride;
        if(at+0x20u>state.size())return std::nullopt;
        auto& v=voices[i];
        v.complete=true;
        v.internal_key_marker=le16(state,at);
        v.blocked_when_few_voices=state[at+0x17u]!=0u;
        v.stored_sample_end=le32(state,at+0x0au);
        const auto voice=static_cast<std::uint8_t>(i);
        v.volume_target_hi16=static_cast<std::uint16_t>(
            readHardware32(chip,3u,voice,0x620u)>>16u);
        chip.writePort(0xe22u,static_cast<std::uint16_t>(
            (5u<<5u)|voice));
        v.envelope_status_reg=chip.readPort(0xa20u);
        v.chip_current_address=readHardware32(chip,0u,voice,0xa20u);
    }
    return candidateOriginalWin95Mode0MonoVoice(
        voices,usable,mode_count);
}
} // namespace awe64::creative
