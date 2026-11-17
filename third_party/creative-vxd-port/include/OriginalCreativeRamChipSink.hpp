#pragma once
// Port transport for the independently reconstructed original Creative
// Win95 SBAWE.VXD PCOD RAM-bank uploader. No DSP fork or PCM conversion here.
// A full transfer still belongs outside the realtime audio callback.
#include "OriginalCreativeSf2RamUpload.hpp"
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

namespace awe64::original_vxd {
// Requires the same 16-bit 125A Chip contract as ChipPortBridge.
// Chip::writePort returns void: a successful return proves only that writes
// were *submitted*, not that a hardware/guest device acknowledged them.
template<class ChipT>
struct OriginalCreativeRamChipSink {
    static_assert(noexcept(std::declval<ChipT&>().writePort(
        std::uint16_t{},std::uint16_t{})));
    static_assert(noexcept(std::declval<const ChipT&>().ready()));

    [[nodiscard]] static bool upload(void* ptr,
                                      std::uint32_t word_address,
                                      std::span<const std::int16_t> words) noexcept {
        if(ptr==nullptr || words.empty())return false;
        auto& chip=*static_cast<ChipT*>(ptr);
        if(!chip.ready() || word_address<0x200000u ||
           word_address>0x00ffffffu ||
           words.size()>static_cast<std::size_t>(0x1000000u-word_address))
            return false;
        // Original guest, on every block: select SMALW index 0x36,
        // issue its 32-bit word address as low A20/high A22, then
        // RESELECT SMLD index 0x3A before EACH individual A20 word.
        // The authentic owner Win95 port trace does not reuse one SMLD
        // selector across a block of multiple words.
        // The chip adapter implements the original 16-bit host port API.
        chip.writePort(0x0e22u,0x0036u);
        chip.writePort(0x0a20u,static_cast<std::uint16_t>(word_address));
        chip.writePort(0x0a22u,static_cast<std::uint16_t>(word_address>>16u));
        for(const auto value:words) {
            chip.writePort(0x0e22u,0x003au);
            chip.writePort(0x0a20u,static_cast<std::uint16_t>(value));
        }
        return true;
    }
    [[nodiscard]] static OriginalSf2RamSink bind(ChipT& chip) noexcept {
        return OriginalSf2RamSink{&chip,upload};
    }
};
} // namespace awe64::original_vxd
