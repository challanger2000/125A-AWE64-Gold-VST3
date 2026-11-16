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
        runtime_.reset();chip_=nullptr;bankSnapshot_={};
        bankBound_=false;source_={};
    }
    // A passed flag alone is NOT authenticity proof. The VST3 owner
    // must first SHA-authenticate its external Creative source tables
    // and exact SF2 descriptors. Never call with synthetic fixtures
    // on a user audio path.
    //
    // Capture the tiny borrowed BANK VIEW by value. The original source
    // callbacks keep an opaque pointer to this instance-owned snapshot,
    // NEVER to a caller stack temporary. The underlying SF2 source,
    // zones, and original VXD 288-slot rows must still be kept immutable
    // and alive until resetNonRealtime() (outside process()).
    [[nodiscard]] bool activate(ChipT& chip,SourceBank bank,
                                bool independentlyAuthenticated) noexcept {
        if(runtime_ || chip_ || bankBound_ || !chip.ready() ||
           !independentlyAuthenticated || bank.zones.empty())return false;
        // When the VXD slot-directory is claimed authentic, every
        // activated SF2 zone must resolve to its exact ORIGINAL slot/row
        // identity. A mere set of 77 PHDR rows is not a 288-slot map.
        const auto& map=bank.original_program_map;
        if(map.authentic_driver_records_verified &&
           map.slots.size()!=awe64::original_vxd::original_program_bank_slots)
            return false;
        for(const auto& zone:bank.zones) {
            if(!zone.original_zone_verified ||
               !zone.voice.source_verified ||
               !zone.voice.note_start_descriptor_verified ||
               !zone.original_state.original_descriptor_verified ||
               !zone.original_state.original_device_state_verified ||
               !zone.original_state.original_sample_placement_verified)
                return false;
            if(map.authentic_driver_records_verified) {
                if(!zone.original_preset_identity_verified ||
                   zone.original_bank_slot>=map.slots.size())
                    return false;
                const auto rows=map.slots[zone.original_bank_slot].rows;
                if(zone.original_preset_row>=rows.size())
                    return false;
                const auto& original=rows[zone.original_preset_row];
                if(original.original_source_marker==0u ||
                   original.original_source_marker==0xffffu ||
                   original.original_bank_byte!=zone.bank ||
                   original.original_program_byte!=zone.program)
                    return false;
                // Reject a truncated original VXD row table. Runtime
                // ProgramChange independently performs its own bounded
                // source-ordered lookup and fallback.
                bool terminated=false;
                for(const auto& row:rows) {
                    if(row.original_source_marker==0xffffu) {
                        terminated=true;break;
                    }
                }
                if(!terminated)return false;
            }
        }
        runtime_.emplace();
        if(!runtime_->bind_chip(chip)) {
            runtime_.reset();return false;
        }
        chip_=&chip;
        bankSnapshot_=bank;
        bankBound_=true;
        source_=awe64::original_vxd::original_verified_sf2_bank_source(
            bankSnapshot_);
        return true;
    }
    [[nodiscard]] bool active() const noexcept {
        return runtime_ && chip_ && bankBound_ &&
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
    // Stable address for the original C++23 callback opaque value.
    // Neither original port nor host consumes a temporary bank object.
    SourceBank bankSnapshot_{};
    bool bankBound_=false;
    Source source_{};
};
} // namespace awe64::vst3
