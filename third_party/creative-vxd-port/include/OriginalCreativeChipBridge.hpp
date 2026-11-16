#pragma once
// Compile-time, heap-free bridge from genuine source MIDI/voice algorithms
// to the existing 125A Chip::writePort/readPort API. No second chip/DSP.
#include "OriginalCreativeVxdLive.hpp"
#include <cstdint>
#include <type_traits>

namespace awe64::original_vxd {
// ChipT's public contract must be:
//   void writePort(uint16_t,uint16_t) noexcept;
//   uint16_t readPort(uint16_t) noexcept;
//   bool ready() const noexcept;
// Calls are serialized against Chip::render by the VST3 owner; this
// bridge intentionally does NOT introduce locks onto the audio thread.
template<class ChipT>
struct ChipPortBridge {
    static_assert(noexcept(std::declval<ChipT&>().writePort(
        std::uint16_t{},std::uint16_t{})));
    static_assert(noexcept(std::declval<ChipT&>().readPort(std::uint16_t{})));
    static_assert(noexcept(std::declval<ChipT const&>().ready()));

    [[nodiscard]] static bool out8(void* opaque,std::uint16_t port,
                                   std::uint8_t value) noexcept {
        auto& chip=*static_cast<ChipT*>(opaque);
        if(!chip.ready())return false;
        chip.writePort(port,static_cast<std::uint16_t>(value));
        return true;
    }
    [[nodiscard]] static bool out16(void* opaque,std::uint16_t port,
                                    std::uint16_t value) noexcept {
        auto& chip=*static_cast<ChipT*>(opaque);
        if(!chip.ready())return false;
        chip.writePort(port,value);
        return true;
    }
    [[nodiscard]] static bool in16(void* opaque,std::uint16_t port,
                                   std::uint16_t& value) noexcept {
        auto& chip=*static_cast<ChipT*>(opaque);
        if(!chip.ready())return false;
        value=chip.readPort(port);
        return true;
    }
    [[nodiscard]] static bool in32(void* opaque,std::uint16_t port,
                                   std::uint32_t& value) noexcept {
        auto& chip=*static_cast<ChipT*>(opaque);
        if(!chip.ready())return false;
        // Original 86Box splits IN EAX,DX into two 16-bit callbacks.
        const auto lo=chip.readPort(port);
        const auto hi=chip.readPort(static_cast<std::uint16_t>(port+2u));
        value=static_cast<std::uint32_t>(lo) |
              (static_cast<std::uint32_t>(hi)<<16u);
        return true;
    }
    [[nodiscard]] static bool out32(void* opaque,std::uint16_t port,
                                    std::uint32_t value) noexcept {
        auto& chip=*static_cast<ChipT*>(opaque);
        if(!chip.ready())return false;
        // Preserve original low-word then high-word DATA0 callbacks.
        chip.writePort(port,static_cast<std::uint16_t>(value));
        chip.writePort(static_cast<std::uint16_t>(port+2u),
                       static_cast<std::uint16_t>(value>>16u));
        return true;
    }
    [[nodiscard]] static PortIo bind(ChipT& chip) noexcept {
        return PortIo{&chip,out8,out16,in16,in32,out32};
    }
};
} // namespace awe64::original_vxd
