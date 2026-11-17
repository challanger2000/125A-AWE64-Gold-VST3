#pragma once
// Owned source SF2 + original VXD directory, compiled on demand through
// the EXISTING reader/resource builder/verified-bank/MIDI engine. No DSP,
// note/velocity expansion, recorded-register replay or fabricated presets.
// prepare/reset/lease operations are NON-RT; callbacks use fixed storage.
#include "OriginalCreativeSf2ResourceBuilder.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
namespace awe64::vst3 {template<class ChipT> class P005OriginalCreativeHostAdapter;}
namespace awe64::original_vxd {
struct OriginalSf2RuntimeFile {
    std::span<const std::uint8_t> bytes{};
    OriginalSf2SourceTables tables{};
};
struct OriginalSf2RuntimePresetBinding {
    bool original_binding_verified=false;
    std::uint16_t device_bank_slot=0,device_preset_row=0;
    std::size_t file_index=0;
    std::uint16_t source_preset_row=0,source_bank=0;
    std::uint8_t source_program=0;
    // Original descriptor +0x90. Not inferred from a VXD child index
    // or PHDR ordinal; the authenticated source loader must supply it.
    std::uint32_t original_descriptor_group=0;
};
struct OriginalSf2RuntimeResources {
    bool original_device_state_verified=false;
    bool original_volume_tables_verified=false;
    bool original_format1_verified=false;
    bool original_format1=false;
    std::uint16_t device_additive_attenuation=0;
    std::uint8_t device_effects_mode=0;
    // Owned original LCOD tables: +2E20, +2EA0, +2F20, +2FA0, +2F82.
    std::array<std::uint8_t,128> velocity{},cc11{},cc7{};
    std::array<std::uint16_t,16> volume_positive16{};
    std::array<std::uint16_t,15> volume_negative15{};
};
enum class Sf2RuntimeBankStatus:std::uint8_t {
    Ok,Unverified,Malformed,BudgetExceeded,AllocationFailed,AlreadyPrepared
};
class OriginalCreativeSf2RuntimeBank final {
public:
    OriginalCreativeSf2RuntimeBank() noexcept;
    OriginalCreativeSf2RuntimeBank(OriginalCreativeSf2RuntimeBank const&)=delete;
    OriginalCreativeSf2RuntimeBank& operator=(OriginalCreativeSf2RuntimeBank const&)=delete;
    OriginalCreativeSf2RuntimeBank(OriginalCreativeSf2RuntimeBank&&)=delete;
    OriginalCreativeSf2RuntimeBank& operator=(OriginalCreativeSf2RuntimeBank&&)=delete;
    // Copies actual source files, all 288 original slot tables, bindings
    // and original resources. Trust flags are assertions by a separately
    // SHA-authenticated loader, never authentication performed here.
    // Budget covers retained object/buffer capacity, not allocator overhead
    // or the independently allocated physical EMU8000 sample RAM.
    [[nodiscard]] Sf2RuntimeBankStatus prepareNonRealtime(
        std::span<const OriginalProgramBankSlot> original_slots,
        std::span<const OriginalSf2RuntimeFile> source_files,
        std::span<const OriginalSf2RuntimePresetBinding> original_bindings,
        OriginalSf2RuntimeResources const& resources,
        std::size_t memory_budget_bytes,bool independent_source_attested) noexcept;
    [[nodiscard]] bool resetNonRealtime() noexcept;
    [[nodiscard]] bool ready() const noexcept {return ready_;}
    [[nodiscard]] bool runtimeLeased() const noexcept {return leased_;}
    [[nodiscard]] std::size_t retainedBytes() const noexcept {return retained_bytes_;}
    [[nodiscard]] std::size_t presetBindingCount() const noexcept {return bindings_.size();}
    [[nodiscard]] std::size_t fileCount() const noexcept {return files_.size();}
    [[nodiscard]] std::size_t unboundPresetCount() const noexcept {return unbound_presets_;}
    [[nodiscard]] Sf2BuildStatus lastNoteStatus() const noexcept {
        return note_status_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint8_t deviceEffectsMode() const noexcept {return resources_.device_effects_mode;}
    [[nodiscard]] SoundFontSource source() noexcept;
    [[nodiscard]] MidiEngine::VolumeSource controllerResources() const noexcept;
private:
    template<class ChipT> friend class awe64::vst3::P005OriginalCreativeHostAdapter;
    [[nodiscard]] bool acquireRuntimeLeaseNonRealtime() noexcept;
    void releaseRuntimeLeaseNonRealtime() noexcept {leased_=false;}
    struct OwnedFile {std::vector<std::uint8_t> bytes{};OriginalSf2SourceTables tables{};};
    [[nodiscard]] static bool resolve(void*,MidiChannel const&,std::uint8_t,
        std::uint8_t,std::array<SourceVoice,max_source_note_layers>&,std::uint8_t&) noexcept;
    [[nodiscard]] static bool compile(void*,SourceVoice const&,std::uint8_t,
        std::uint8_t,OriginalVoiceProgram&) noexcept;
    [[nodiscard]] static bool selectProgram(void*,MidiChannel const&,
        std::uint8_t,OriginalProgramSelection&) noexcept;
    [[nodiscard]] OriginalProgramBankMap programMap() const noexcept {
        return {ready_,slots_};
    }
    std::array<std::vector<OriginalPresetTableRow>,original_program_bank_slots> rows_{};
    std::array<OriginalProgramBankSlot,original_program_bank_slots> slots_{};
    std::vector<OwnedFile> files_{};
    std::vector<OriginalSf2RuntimePresetBinding> bindings_{};
    OriginalSf2RuntimeResources resources_{};
    OriginalSf2PreparedBank cache_{};
    VerifiedSf2Bank cache_view_{};
    std::size_t retained_bytes_=0;
    std::size_t unbound_presets_=0;
    std::uint64_t generation_=0;
    std::uint8_t cached_key_=0;
    static_assert(std::atomic<Sf2BuildStatus>::is_always_lock_free);
    std::atomic<Sf2BuildStatus> note_status_{Sf2BuildStatus::Unverified};
    bool ready_=false,leased_=false;
};
} // namespace awe64::original_vxd
