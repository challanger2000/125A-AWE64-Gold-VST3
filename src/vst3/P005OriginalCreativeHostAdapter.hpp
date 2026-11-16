#pragma once
// P005: Host/VST3 -> ORIGINAL Creative C++23 port, NOT a second engine.
// Native MIDI and RAM uploads share the exact same DeviceState and Chip.
// SoundFont descriptors must be independently authenticated by the caller.
// No original chip DSP, guessed controller formula or CPU timeline here.
#include "OriginalCreativeMidiRamRuntime.hpp"
#include "OriginalCreativeVerifiedSf2Bank.hpp"
#include <cstdint>
#include <memory>
#include <optional>
namespace awe64::vst3 {
template<class ChipT>
class P005OriginalCreativeHostAdapter final {
public:
    using Runtime=awe64::original_vxd::OriginalCreativeMidiRamRuntime;
    using Result=awe64::original_vxd::MidiResult;
    using SourceBank=awe64::original_vxd::VerifiedSf2Bank;
    using Source=awe64::original_vxd::SoundFontSource;
    P005OriginalCreativeHostAdapter()=default;
    P005OriginalCreativeHostAdapter(
        const P005OriginalCreativeHostAdapter&)=delete;
    P005OriginalCreativeHostAdapter& operator=(
        const P005OriginalCreativeHostAdapter&)=delete;
    // OFF THE AUDIO THREAD, after physical voices have stopped.
    void resetNonRealtime() noexcept {
        runtime_.reset();chip_=nullptr;bank_=nullptr;source_={};
    }
    // A passed flag alone is NOT authenticity proof. The VST3 owner
    // must first SHA-authenticate its external Creative source tables
    // and exact SF2 descriptors. Never call with synthetic fixtures
    // on a user audio path.
    [[nodiscard]] bool activate(ChipT& chip,SourceBank& bank,
                                bool independentlyAuthenticated) noexcept {
        if(runtime_ || chip_ || bank_ || !chip.ready() ||
           !independentlyAuthenticated || bank.zones.empty())return false;
        for(const auto& zone:bank.zones)
            if(!zone.original_zone_verified ||
               !zone.voice.source_verified ||
               !zone.voice.note_start_descriptor_verified ||
               !zone.original_state.original_descriptor_verified ||
               !zone.original_state.original_device_state_verified ||
               !zone.original_state.original_sample_placement_verified)
                return false;
        runtime_.emplace();
        if(!runtime_->bind_chip(chip)) {
            runtime_.reset();return false;
        }
        chip_=&chip;bank_=&bank;
        source_=awe64::original_vxd::original_verified_sf2_bank_source(bank);
        return true;
    }
    [[nodiscard]] bool active() const noexcept {
        return runtime_ && chip_ && bank_ &&
               runtime_->attached_to(*chip_) &&
               !runtime_->midi.fault && !runtime_->scratch.faulted;
    }
    [[nodiscard]] Runtime* sharedRuntime() noexcept {
        return runtime_?std::addressof(*runtime_):nullptr;
    }
    [[nodiscard]] Result midi(std::uint8_t status,std::uint8_t d1,
                              std::uint8_t d2=0u) noexcept {
        if(!active())return Result::MissingSource;
        // The original driver port itself determines unsupported MIDI
        // operations. No fabricated or silent fallback implementation.
        return runtime_->midi_event(*chip_,status,d1,d2,source_);
    }
    [[nodiscard]] Result noteOn(std::uint8_t ch,std::uint8_t pitch,
                                std::uint8_t velocity) noexcept {
        if(ch>=16u || pitch>=128u || velocity>=128u)
            return Result::Invalid;
        return midi(static_cast<std::uint8_t>(0x90u|ch),pitch,velocity);
    }
    [[nodiscard]] Result noteOff(std::uint8_t ch,std::uint8_t pitch,
                                 std::uint8_t velocity=0u) noexcept {
        if(ch>=16u || pitch>=128u || velocity>=128u)
            return Result::Invalid;
        return midi(static_cast<std::uint8_t>(0x80u|ch),pitch,velocity);
    }
    [[nodiscard]] Result program(std::uint8_t ch,std::uint8_t number) noexcept {
        if(ch>=16u || number>=128u)return Result::Invalid;
        return midi(static_cast<std::uint8_t>(0xc0u|ch),number);
    }
    [[nodiscard]] Result controller(std::uint8_t ch,std::uint8_t id,
                                    std::uint8_t value) noexcept {
        if(ch>=16u || id>=128u || value>=128u)
            return Result::Invalid;
        return midi(static_cast<std::uint8_t>(0xb0u|ch),id,value);
    }
    [[nodiscard]] Result pitchBend(std::uint8_t ch,
                                   std::uint16_t bend14) noexcept {
        if(ch>=16u || bend14>=16384u)return Result::Invalid;
        return midi(static_cast<std::uint8_t>(0xe0u|ch),
                    static_cast<std::uint8_t>(bend14&127u),
                    static_cast<std::uint8_t>(bend14>>7u));
    }
private:
    std::optional<Runtime> runtime_{};
    ChipT* chip_=nullptr;
    SourceBank* bank_=nullptr;
    Source source_{};
};
} // namespace awe64::vst3
