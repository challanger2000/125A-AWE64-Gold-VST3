#pragma once
// P005: own and publish the EXACT authenticated source VXD 288-slot
// program directory plus precompiled ORIGINAL Creative SF2 voice zones.
// Not a second SoundFont parser, oscillator, SF2 modifier or MIDI engine.
// Only existing OriginalCreativeSf2NoteStage and OriginalProgramBankMap
// source-derived inputs are accepted. Private Creative/SF2 bytes stay out.
//
// All mutations are non-realtime and may allocate; after seal() only
// stable read-only spans are published to the original MIDI dispatcher.
// C++ type is intentionally non-copyable/non-movable: its spans must
// never dangle after VST3 callback activation.
#include "P005OriginalCreativeSf2NoteStage.hpp"
#include "OriginalCreativeVerifiedSf2Bank.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace awe64::vst3 {
class P005OriginalCreativeBankCatalog final {
public:
    using Row=awe64::original_vxd::OriginalPresetTableRow;
    using Slot=awe64::original_vxd::OriginalProgramBankSlot;
    using Zone=awe64::original_vxd::VerifiedSf2Zone;
    using Bank=awe64::original_vxd::VerifiedSf2Bank;
    static constexpr std::size_t kSourceSlots=
        awe64::original_vxd::original_program_bank_slots;
    // A clear bounded research limit, not a fake 128-preset count.
    // Need more authentic entries? Raise this bound with memory tests.
    static constexpr std::size_t kMaxVerifiedZones=16384u;
    static constexpr std::size_t kMaxRowsPerSlot=65536u;

    P005OriginalCreativeBankCatalog()=default;
    P005OriginalCreativeBankCatalog(P005OriginalCreativeBankCatalog const&)=delete;
    P005OriginalCreativeBankCatalog&
        operator=(P005OriginalCreativeBankCatalog const&)=delete;
    P005OriginalCreativeBankCatalog(P005OriginalCreativeBankCatalog&&)=delete;
    P005OriginalCreativeBankCatalog&
        operator=(P005OriginalCreativeBankCatalog&&)=delete;

    // NON-RT ONLY. Caller must independently authenticate EVERY original
    // row and table as derived from the Creative Win95 driver. The boolean
    // is a required policy assertion, NOT authentication by this class.
    [[nodiscard]] bool importAuthenticatedOriginalDirectory(
        std::span<const Slot> authenticSource,
        bool sourceAttestationPassed) {
        if(sealed_ || directoryReady_ || !zones_.empty() ||
           !sourceAttestationPassed || authenticSource.size()!=kSourceSlots)
            return false;
        // Validate ALL rows before publishing any borrowed view.
        for(const auto& source:authenticSource) {
            if(source.rows.size()>kMaxRowsPerSlot)return false;
            if(!source.rows.empty()) {
                if(source.rows.back().original_source_marker!=0xffffu)
                    return false;
                // The Creative VXD ends a slot at its FIRST sentinel.
                // Accepting an earlier sentinel followed by extra rows
                // would leave an ambiguous and potentially forged table.
                for(std::size_t i=0u;i+1u<source.rows.size();++i)
                    if(source.rows[i].original_source_marker==0xffffu)
                        return false;
            }
        }
        // These vectors are now the OWNER of actual original rows,
        // not the caller's temporary/per-file import buffers.
        for(std::size_t i=0u;i<kSourceSlots;++i)
            ownedRows_[i].assign(authenticSource[i].rows.begin(),
                                 authenticSource[i].rows.end());
        for(std::size_t i=0u;i<kSourceSlots;++i)
            slotViews_[i].rows=ownedRows_[i];
        directoryReady_=true;
        return true;
    }

    // Called after original SF2 reader + original resource compiler have
    // produced ALL descriptors for this one note context. Copy the actual
    // original encoded voices, NEVER synthesize a missing PHDR program.
    [[nodiscard]] bool appendVerifiedNoteContext(
        P005OriginalCreativeSf2NoteStage const& stage) {
        if(sealed_ || !directoryReady_ || !stage.ready() ||
           stage.count()==0u || zones_.size()+stage.count()>kMaxVerifiedZones)
            return false;
        const auto view=stage.view();
        if(view.zones.size()!=stage.count())return false;
        // Source identity preflight must complete on all note layers
        // before the vector can grow. Rejecting one layer is atomic.
        for(auto const& z:view.zones) {
            if(!zoneMatchesOriginalDirectory(z) ||
               z.key_low!=stage.key() || z.key_high!=stage.key() ||
               z.velocity_low!=stage.velocity() ||
               z.velocity_high!=stage.velocity() ||
               z.bank!=stage.sf2Bank() ||
               z.program!=stage.sf2Program())
                return false;
        }
        zones_.insert(zones_.end(),view.zones.begin(),view.zones.end());
        return true;
    }

    // Verify every stored voice again and lock the entire source bank.
    // Caller is responsible for original private SHA/driver provenance.
    [[nodiscard]] bool sealNonRealtime() noexcept {
        if(sealed_ || !directoryReady_ || zones_.empty())return false;
        for(auto const& z:zones_)
            if(!zoneMatchesOriginalDirectory(z))return false;
        sealed_=true;return true;
    }

    [[nodiscard]] bool ready() const noexcept {
        return sealed_ && directoryReady_ && !zones_.empty();
    }
    [[nodiscard]] std::size_t zoneCount() const noexcept {
        return zones_.size();
    }
    [[nodiscard]] std::size_t sourceSlotCount() const noexcept {
        return directoryReady_?kSourceSlots:0u;
    }
    // The adapter copies only this cheap view; its zones and 288 row
    // pointers remain valid as long as this sealed owner lives unchanged.
    [[nodiscard]] Bank view() const noexcept {
        Bank v{};
        if(!ready())return v;
        v.zones=zones_;
        v.original_program_map={true,slotViews_};
        return v;
    }

    // OFF audio thread, AFTER original MIDI runtime has been detached.
    void resetNonRealtime() {
        sealed_=false;directoryReady_=false;
        zones_.clear();
        for(std::size_t i=0u;i<kSourceSlots;++i) {
            slotViews_[i].rows={};
            ownedRows_[i].clear();
        }
    }

private:
    [[nodiscard]] bool zoneMatchesOriginalDirectory(
        Zone const& z) const noexcept {
        if(!directoryReady_ || !z.original_zone_verified ||
           !z.original_preset_identity_verified ||
           !z.voice.source_verified ||
           !z.voice.note_start_descriptor_verified ||
           !z.original_state.original_descriptor_verified ||
           !z.original_state.original_device_state_verified ||
           !z.original_state.original_sample_placement_verified ||
           z.original_bank_slot>=kSourceSlots)return false;
        auto const rows=slotViews_[z.original_bank_slot].rows;
        if(z.original_preset_row>=rows.size())return false;
        auto const& original=rows[z.original_preset_row];
        return original.original_source_marker!=0u &&
               original.original_source_marker!=0xffffu &&
               original.original_bank_byte==z.bank &&
               original.original_program_byte==z.program;
    }

    std::array<std::vector<Row>,kSourceSlots> ownedRows_{};
    std::array<Slot,kSourceSlots> slotViews_{};
    std::vector<Zone> zones_{};
    bool directoryReady_=false,sealed_=false;
};
} // namespace awe64::vst3
