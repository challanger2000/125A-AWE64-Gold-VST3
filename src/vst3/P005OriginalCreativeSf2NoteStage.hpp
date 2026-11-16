#pragma once
// P005: non-RT, original-SF2-source-to-original-voice staging boundary.
// Reuses the Control Chat's original C++23 RIFF layer selector and the
// Creative resource/descriptor compiler WITHOUT another MIDI/DSP engine.
// No automatic 128-preset fill and no inferred Creative device bank map.
// Original resource tables, pitch values and RAM physical placement MUST
// be authenticated externally for THIS specific bank and note context.
// Stages ONE bank/program/key/velocity selection, not all presets/notes.
// Storage belongs to this class; do not copy/move it while VST3 uses view().
#include "OriginalCreativeSf2SourceReader.hpp"
#include "OriginalCreativeSf2ResourceBuilder.hpp"
#include "OriginalCreativeVerifiedSf2Bank.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace awe64::vst3 {
class P005OriginalCreativeSf2NoteStage final {
public:
    using Status=awe64::original_vxd::Sf2BuildStatus;
    P005OriginalCreativeSf2NoteStage()=default;
    P005OriginalCreativeSf2NoteStage(P005OriginalCreativeSf2NoteStage const&)=delete;
    P005OriginalCreativeSf2NoteStage& operator=(P005OriginalCreativeSf2NoteStage const&)=delete;
    P005OriginalCreativeSf2NoteStage(P005OriginalCreativeSf2NoteStage&&)=delete;
    P005OriginalCreativeSf2NoteStage& operator=(P005OriginalCreativeSf2NoteStage&&)=delete;

    // This function only runs on activation/preparation threads with the
    // caller-owned SF2, ORIGINAL source templates and channel state held
    // stable. The data is prepared by the ORIGINAL resource builder.
    [[nodiscard]] Status prepareNonRealtime(
        std::span<const std::uint8_t> sf2,
        std::uint16_t sf2Bank,std::uint8_t sf2Program,
        std::uint8_t key,std::uint8_t velocity,
        awe64::original_vxd::OriginalSf2SourceTables const& verifiedTables,
        std::span<const awe64::original_vxd::OriginalSf2SourceChannelState> verifiedLayers,
        bool externallyAttestedSource) noexcept {
        clearNonRealtime();
        if(!externallyAttestedSource || !verifiedTables.original_generator_templates_verified ||
           !verifiedTables.original_pitch_tables_verified ||
           !verifiedTables.original_sample_ram_placement_verified)
            return Status::Unverified;
        if(sf2Program>=128u || key>=128u || velocity==0u || velocity>=128u)
            return Status::Malformed;
        auto layers=awe64::original_vxd::select_sf2_raw_layers(
            sf2,sf2Bank,sf2Program,key,velocity);
        switch(layers.status) {
            case awe64::original_vxd::Sf2ReadStatus::Malformed:return Status::Malformed;
            case awe64::original_vxd::Sf2ReadStatus::Unsupported:return Status::Unsupported;
            case awe64::original_vxd::Sf2ReadStatus::TooManyLayers:return Status::TooManyLayers;
            case awe64::original_vxd::Sf2ReadStatus::NoMatch:return Status::Malformed;
            case awe64::original_vxd::Sf2ReadStatus::Ok:break;
        }
        if(layers.count==0u || layers.count>zones_.size() ||
           verifiedLayers.size()!=layers.count)return Status::Unverified;
        // Each original descriptor belongs to a concrete PHDR row and
        // precisely the key/velocity for which its source was verified.
        for(std::size_t i=0;i<layers.count;++i) {
            if(layers.layers[i].bank!=sf2Bank ||
               layers.layers[i].program!=sf2Program ||
               layers.layers[i].key!=key ||
               layers.layers[i].velocity!=velocity ||
               !verifiedLayers[i].original_program_identity_verified ||
               !verifiedLayers[i].original_note_context_verified)
                return Status::Unverified;
        }
        const auto prepared=awe64::original_vxd::prepare_original_sf2_zones(
            layers,verifiedTables,verifiedLayers);
        if(prepared.status!=Status::Ok)return prepared.status;
        if(prepared.count!=layers.count)return Status::Malformed;
        for(std::size_t i=0;i<layers.count;++i) {
            const auto& zone=prepared.zones[i];
            if(!zone.original_zone_verified || !zone.voice.source_verified ||
               !zone.voice.note_start_descriptor_verified ||
               !zone.original_state.original_descriptor_verified ||
               !zone.original_state.original_device_state_verified ||
               !zone.original_state.original_sample_placement_verified)
                return Status::Unverified;
        }
        std::copy_n(prepared.zones.begin(),prepared.count,zones_.begin());
        count_=prepared.count;
        sf2Bank_=sf2Bank;sf2Program_=sf2Program;
        key_=key;velocity_=velocity;
        return Status::Ok;
    }
    void clearNonRealtime() noexcept {
        count_=0u;sf2Bank_=0u;sf2Program_=0u;key_=0u;velocity_=0u;
    }
    [[nodiscard]] bool ready() const noexcept {return count_!=0u;}
    [[nodiscard]] std::uint8_t count() const noexcept {return count_;}
    [[nodiscard]] std::uint16_t sf2Bank() const noexcept {return sf2Bank_;}
    [[nodiscard]] std::uint8_t sf2Program() const noexcept {return sf2Program_;}
    [[nodiscard]] std::uint8_t key() const noexcept {return key_;}
    [[nodiscard]] std::uint8_t velocity() const noexcept {return velocity_;}
    // View is borrowed and valid until next clear/prepare/destruction.
    // Original 288-slot map must be supplied by the authentic VXD loader,
    // not fabricated from the 77 SF2 PHDR entries.
    [[nodiscard]] awe64::original_vxd::VerifiedSf2Bank view(
        awe64::original_vxd::OriginalProgramBankMap sourceMap={}) const noexcept {
        awe64::original_vxd::VerifiedSf2Bank v{};
        if(!ready())return v;
        v.zones=std::span<const awe64::original_vxd::VerifiedSf2Zone>(
            zones_.data(),count_);
        v.original_program_map=sourceMap;
        return v;
    }
private:
    std::array<awe64::original_vxd::VerifiedSf2Zone,
               awe64::original_vxd::sf2_max_selected_layers> zones_{};
    std::uint8_t count_=0u;
    std::uint16_t sf2Bank_=0u;
    std::uint8_t sf2Program_=0u,key_=0u,velocity_=0u;
};
} // namespace awe64::vst3
