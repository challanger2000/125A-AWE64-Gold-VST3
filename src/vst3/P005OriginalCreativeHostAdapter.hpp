#pragma once
// P005: Host/VST3 -> ORIGINAL Creative C++23 port, NOT a second engine.
// Native MIDI and RAM uploads share the exact same DeviceState and Chip.
// SoundFont descriptors must be independently authenticated by the caller.
// No original chip DSP, guessed controller formula or CPU timeline here.
#include "OriginalCreativeMidiRamRuntime.hpp"
#include "OriginalCreativeVerifiedSf2Bank.hpp"
#include "P005OriginalCreativeBankCatalog.hpp"
#include "P005OriginalVxdGuestCaptureFile.hpp"
#include <cstdint>
#include <cstddef>
#include <span>
#include <memory>
#include <new>
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
    // The VST3 catalog is declared BEFORE this adapter and thus lives
    // longer during C++ member teardown. Release its source lease even
    // if the host closes without the normal setActive(false) callback.
    ~P005OriginalCreativeHostAdapter() {resetNonRealtime();}
    P005OriginalCreativeHostAdapter(
        const P005OriginalCreativeHostAdapter&)=delete;
    P005OriginalCreativeHostAdapter& operator=(
        const P005OriginalCreativeHostAdapter&)=delete;
    // OFF THE AUDIO THREAD, after physical voices have stopped.
    void resetNonRealtime() noexcept {
        // First detach all original MIDI callbacks, then release the
        // owner lease. No catalog vector may be freed while the live
        // Creative source contains non-owning spans into that catalog.
        runtime_.reset();chip_=nullptr;bankSnapshot_={};
        bankBound_=false;source_={};
        if(ownedCatalog_) {
            ownedCatalog_->releaseRuntimeLeaseNonRealtime();
            ownedCatalog_=nullptr;
        }
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
    // Strict non-realtime entry point for the FUTURE source-authenticated
    // VST3 production loader. In contrast to activate()'s low-level
    // research fixture interface this requires a sealed, OWNED original
    // 288-slot directory and an externally authenticated source pack.
    // Source checksum provenance itself is verified by the loader, not
    // by this adapter, and must never be represented by a fabricated flag.
    [[nodiscard]] bool activateOwnedOriginalCatalogNonRealtime(
        ChipT& chip,P005OriginalCreativeBankCatalog& catalog,
        bool independentShaAttestation) noexcept {
        if(!independentShaAttestation || !catalog.ready() ||
           catalog.sourceSlotCount()!=
              awe64::original_vxd::original_program_bank_slots)
            return false;
        const auto view=catalog.view();
        if(!view.original_program_map.authentic_driver_records_verified ||
           view.original_program_map.slots.size()!=
              awe64::original_vxd::original_program_bank_slots)
            return false;
        if(!catalog.acquireRuntimeLeaseNonRealtime())return false;
        if(!activate(chip,view,true)) {
            catalog.releaseRuntimeLeaseNonRealtime();
            return false;
        }
        ownedCatalog_=&catalog;
        return true;
    }
    // Non-RT native bridge from a separately attested ORIGINAL Win95
    // 0x1592-byte device image to the SAME device used by the original
    // MIDI/allocator/RAM runtime. No parallel synthetic voice state.
    //
    // Source offsets: device+0x42, stride 0x20, 32 physical voices;
    // device+0x06 is physical capacity; +0x18 is the mutable mode
    // count. All values are copied from the actual owner snapshot, not
    // inferred from a preset, PHDR or the separate 30-voice helper.
    // Only selected program/row/slot/bank fields are restored for
    // source-matching channels. Remaining CC/RPN state is still unknown;
    // product startup needs independently verified full channel history.
    [[nodiscard]] bool activateOwnedOriginalCatalogWithDeviceNonRealtime(
        ChipT& chip,P005OriginalCreativeBankCatalog& catalog,
        std::span<const std::uint8_t> originalDeviceImage,
        bool independentGuestSessionAttested) noexcept {
        constexpr std::size_t imageBytes=0x1592u;
        constexpr std::size_t voiceStart=0x42u;
        constexpr std::size_t voiceStride=0x20u;
        constexpr std::size_t nVoices=awe64::original_vxd::physical_voices;
        static_assert(voiceStart+nVoices*voiceStride<=imageBytes);
        if(!independentGuestSessionAttested ||
           originalDeviceImage.size()!=imageBytes || sourceBound())
            return false;
        const auto read16=[originalDeviceImage](std::size_t offset) noexcept {
            return static_cast<std::uint16_t>(
                std::uint16_t{originalDeviceImage[offset]} |
                (std::uint16_t{originalDeviceImage[offset+1u]}<<8u));
        };
        const auto read32=[&read16](std::size_t offset) noexcept {
            return std::uint32_t{read16(offset)} |
                   (std::uint32_t{read16(offset+2u)}<<16u);
        };
        if(read32(0x158eu)!=0x4d474d31u ||
           read16(0x06u)!=nVoices)
            return false;
        awe64::original_vxd::DeviceState restored{};
        restored.physical_capacity=read16(0x06u);
        restored.mode_count=read32(0x18u);
        restored.original_pan_device_mode=read16(0x12u);
        restored.original_pan_device_mode_verified=true;
        for(std::size_t i=0u;i<nVoices;++i) {
            const auto off=voiceStart+i*voiceStride;
            auto& v=restored.voices[i];
            v.key_marker=read16(off);
            v.release_sample_end=read32(off+0x06u);
            v.sample_end=read32(off+0x0au);
            v.original_pitch_base=read16(off+0x0eu);
            v.original_pitch_base_verified=true;
            v.original_amplitude_word=
                static_cast<std::int16_t>(read16(off+0x04u));
            v.original_modulation_word_signed=
                static_cast<std::int8_t>(originalDeviceImage[off+0x14u]);
            v.original_modulation_word_verified=true;
            v.modulation_release=originalDeviceImage[off+0x15u];
            v.volume_release=originalDeviceImage[off+0x16u];
            v.blocked=originalDeviceImage[off+0x17u]!=0u;
            v.pair_penalty=(originalDeviceImage[off+0x1au]&1u)!=0u;
            v.original_pan_base=originalDeviceImage[off+0x12u];
            v.pan_target=originalDeviceImage[off+0x18u];
            v.pan_current=originalDeviceImage[off+0x19u];
            v.pan_state_verified=true;
            // Original voice+0x02 is NOT a verified volume lookup
            // result. Never assert volume_terms_verified from raw bytes.
        }
        // Original LCOD+6D05 persists resolved ProgramChange state in
        // each source channel row (device+442h, stride 24h):
        // +00h=VXD row, +02h=program, +18h=bank slot, +1Bh=bank byte.
        // Import only selections whose exact source row also exists in
        // the independently authenticated catalog. Empty initial rows
        // are NOT synthetic bank-zero programs.
        std::array<awe64::original_vxd::MidiChannel,16u> channels{};
        constexpr std::size_t channelStart=0x442u;
        constexpr std::size_t channelStride=0x24u;
        static_assert(channelStart+16u*channelStride<=imageBytes);
        for(std::size_t ch=0u;ch<channels.size();++ch) {
            const auto offset=channelStart+ch*channelStride;
            const auto row=read16(offset);
            const auto program=read16(offset+0x02u);
            const auto slot=read16(offset+0x18u);
            const auto bank=originalDeviceImage[offset+0x1bu];
            if(program>255u || slot>=
                   P005OriginalCreativeBankCatalog::kSourceSlots)
                continue; // no authenticated mapping for this row
            const auto source=catalog.originalSourceRowNonRealtime(
                slot,row);
            if(!source || source->original_source_marker==0u ||
               source->original_source_marker==0xffffu ||
               source->original_bank_byte!=bank ||
               source->original_program_byte!=program)
                continue;
            auto& state=channels[ch];
            state.original_program_selection_valid=true;
            state.original_selected_bank_slot=slot;
            state.original_selected_preset_row=row;
            state.original_selected_program=
                static_cast<std::uint8_t>(program);
            state.original_selected_bank=bank;
            state.program=static_cast<std::uint8_t>(program);
            // The CC0 *requested* bank need not equal the currently
            // resolved bank. Do not manufacture bank_msb from the
            // selected program record.
        }
        if(!activateOwnedOriginalCatalogNonRealtime(
               chip,catalog,independentGuestSessionAttested))
            return false;
        // scratch.device already points to this exact stable member.
        // No processing can run until activation returns.
        runtime_->midi.device=restored;
        runtime_->midi.channel=channels;
        return true;
    }
    // Complete non-RT SOURCE PREPARATION chain for the exported native
    // VST3 host adapter:
    // owner Win95 125AVXD1 -> exact 288-slot VXD records ->
    // previously original-compiled note contexts -> sealed owner ->
    // original guest device rows -> SAME native Creative MIDI/chip.
    //
    // The two independent attestations are supplied by the private
    // owner-side verifier. File magic or synthetic tests do NOT confer
    // them. Incomplete channel/CPU provenance still prevents normal
    // product setActive() from calling this opt-in entry point.
    [[nodiscard]] bool activateOriginalGuestCaptureBundleNonRealtime(
        ChipT& chip,P005OriginalCreativeBankCatalog& catalog,
        const P005OriginalVxdGuestCaptureFile& guest,
        std::span<const P005OriginalCreativeSf2NoteStage* const> contexts,
        bool originalGuestSessionIndependentlyAttested,
        bool originalNoteContextsIndependentlyAttested) noexcept {
        if(sourceBound() || catalog.runtimeLeased() ||
           catalog.sourceSlotCount()!=0u || catalog.zoneCount()!=0u ||
           !guest.sourceFormatReady() || contexts.empty() ||
           !originalGuestSessionIndependentlyAttested ||
           !originalNoteContextsIndependentlyAttested)
            return false;
        using ImportResult=P005OriginalVxdDirectoryImportResult;
        if(guest.importIntoNonRealtime(
               catalog,originalGuestSessionIndependentlyAttested)!=
           ImportResult::Ok)
            return false;
        bool complete=true;
        try {
            for(const auto* stage:contexts) {
                if(!stage || !stage->ready() ||
                   !catalog.appendVerifiedNoteContext(*stage)) {
                    complete=false;
                    break;
                }
            }
            if(complete)complete=catalog.sealNonRealtime();
            if(complete)
                complete=activateOwnedOriginalCatalogWithDeviceNonRealtime(
                    chip,catalog,guest.deviceImageNonRealtime(),
                    originalGuestSessionIndependentlyAttested);
        }catch(const std::bad_alloc&) {
            complete=false;
        }
        if(!complete) {
            // There must be no partially published source on failure.
            // A successful binding owns the only catalog lease.
            (void)catalog.resetNonRealtime();
        }
        return complete;
    }
    // A latched source-driver or physical chip fault MUST NOT cause the
    // VST3 callback to fall back to the unrelated P001/P002 renderer.
    // Unlike active(), sourceBound() remains true until non-RT reset.
    [[nodiscard]] bool sourceBound() const noexcept {
        return runtime_.has_value() && chip_!=nullptr && bankBound_;
    }
    [[nodiscard]] bool active() const noexcept {
        return sourceBound() &&
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
    // Non-owning pointer; original source catalog is physically owned
    // by the VST3 instance and must outlive this adapter.
    P005OriginalCreativeBankCatalog* ownedCatalog_=nullptr;
};
} // namespace awe64::vst3
