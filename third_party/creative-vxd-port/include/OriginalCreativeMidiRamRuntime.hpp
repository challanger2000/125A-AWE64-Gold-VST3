#pragma once
// Glue, not a new Creative DSP/algorithm: one original VXD DeviceState
// shared by the portable MIDI dispatcher AND the PCOD scratch RAM loader.
// The scratch allocator MUST see MIDI-occupied physical voices before an
// overlapping bank load; constructing a second DeviceState loses that fact.
// Calls are serialized OFF the audio thread during bank upload. Does not
// model Windows 95 TSC timing or guarantee native PCM/audio equivalence.
#include "OriginalCreativeMidiEngine.hpp"
#include "OriginalCreativeRamScratchVoices.hpp"
#include "OriginalCreativeRamChipSink.hpp"
#include "OriginalCreativeChipBridge.hpp"
#include <cstdint>
#include <span>

namespace awe64::original_vxd {
struct OriginalCreativeMidiRamRuntime final {
    MidiEngine midi{};
    OriginalCreativeRamScratchCycle scratch{};

    OriginalCreativeMidiRamRuntime() noexcept = default;
    OriginalCreativeMidiRamRuntime(OriginalCreativeMidiRamRuntime const&) = delete;
    OriginalCreativeMidiRamRuntime& operator=(OriginalCreativeMidiRamRuntime const&) = delete;
    OriginalCreativeMidiRamRuntime(OriginalCreativeMidiRamRuntime&&) = delete;
    OriginalCreativeMidiRamRuntime& operator=(OriginalCreativeMidiRamRuntime&&) = delete;

    template<class ChipT>
    [[nodiscard]] bool bind_chip(ChipT& chip) noexcept {
        // Never replace the active state/chip during or after an upload;
        // the latter would leave physical RAM/registers in the old chip.
        if(!chip.ready() || scratch.active || scratch.faulted || midi.fault ||
           scratch.cycles_completed!=0u || scratch.io.opaque!=nullptr)
            return false;
        scratch.device=&midi.device;
        scratch.io=ChipPortBridge<ChipT>::bind(chip);
        return true;
    }

    template<class ChipT>
    [[nodiscard]] bool attached_to(ChipT& chip) const noexcept {
        return scratch.device==&midi.device &&
               scratch.io.opaque==static_cast<void*>(&chip) &&
               scratch.io.out8!=nullptr && scratch.io.out32!=nullptr &&
               scratch.io.in16!=nullptr && scratch.io.in32!=nullptr;
    }

    template<class ChipT>
    [[nodiscard]] MidiResult midi_event(ChipT& chip, std::uint8_t status,
                                        std::uint8_t d1,std::uint8_t d2,
                                        SoundFontSource source) noexcept {
        if(!attached_to(chip))return MidiResult::MissingSource;
        if(scratch.active || scratch.faulted || midi.fault)
            return MidiResult::FaultLatched;
        if(!chip.ready())return MidiResult::PortError;
        return dispatch_midi(midi,status,d1,d2,scratch.io,source);
    }

    template<class ChipT>
    [[nodiscard]] OriginalCreativeRamUploadResult upload_bank(
        ChipT& chip,std::span<const std::uint8_t> source_sf2,
        OriginalCreativeRamBankCursor& cursor) noexcept {
        if(!attached_to(chip) || scratch.active || scratch.faulted ||
           midi.fault || cursor.faulted)
            return {};
        const auto result=upload_original_creative_sf2_ram_bank(
            source_sf2,cursor,OriginalCreativeRamChipSink<ChipT>::bind(chip),
            scratch.hooks());
        if(result.status==OriginalSf2UploadStatus::UploadFailed ||
           scratch.faulted)midi.fault=true;
        return result;
    }
};
} // namespace awe64::original_vxd
