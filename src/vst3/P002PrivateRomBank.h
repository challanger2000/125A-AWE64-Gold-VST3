#pragma once
// P002 RESEARCH ONLY: opt-in, source-backed ROM-mono SF2 bank inside the
// actual exported Windows VST3. The original 1996 SHA-pinned SBAWE.VXD
// source resources and the full source-owned initial device state arrive
// through a PRIVATE locally generated binary pack. Never embed, publish,
// or distribute that pack or Creative bank/ROM contents.
//
// Loading and parsing input files happen ONLY on setActive() (non-RT).
// Per-note ROM bank parsing and the original hardware allocator are
// bounded and allocation-free, but this is not yet validated for full
// production-grade 44.1 kHz callback worst-case timing. No fake DSP,
// RAM SoundFont upload, voice stealing, retrigger, or Win95 PCM parity.
#include "CreativeSf2RomBankNoteCompiler.h"
#include "CreativeMidi.h"
#include "CreativeSf2PresetInventory.h"
#include "CreativeSf2RamBankUpload.h"
#ifdef AWE64_P005_ORIGINAL_CREATIVE_LINKED
#include "OriginalCreativeSf2ResourceBuilder.hpp"
#endif
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>
namespace awe64::vst3 {
class P002PrivateRomBank {
public:
    [[nodiscard]] bool load(const std::filesystem::path& source_pack,
                            const std::filesystem::path& soundfont,
                            std::string& error,
                            bool original_ram_source_already_sha_authenticated=false,
                            const awe64::creative::P003RamUploadResult*
                                original_source_captured_ram=nullptr);
    void clear() noexcept;
    [[nodiscard]] bool ready() const noexcept {return ready_;}
    [[nodiscard]] bool originalRamMode() const noexcept {return ram_mode_;}
#ifdef AWE64_P005_ORIGINAL_CREATIVE_LINKED
    // Off-RT: bridge the EXISTING SHA-gated source VXD templates/pitch
    // and independently attested captured RAM position to the port.
    // A successful P002 owner-RAM load is required; the original VXD
    // 288 program-slot directory and per-note voice state are NOT
    // fabricated by this function.
    [[nodiscard]] bool exportVerifiedCreativeP005SourceTables(
        awe64::original_vxd::OriginalSf2SourceTables& out) const noexcept;
    // Real 0x1592-byte VXD guest DEVICE image, including the 288
    // ORIGINAL guest-virtual bank pointers at device+0x6CA.
    // This does NOT provide the pointed-to external guest memory.
    // Source-pack device bytes are NOT independently SHA-attested as
    // guest memory. The exported image is only a candidate.
    // On a failed source/length gate, the entire output is zeroed.
    [[nodiscard]] bool exportP005UnattestedVxdDeviceImage(
        std::span<std::uint8_t> out) const noexcept;
#endif
    [[nodiscard]] std::uint8_t channel() const noexcept {return channel_;}
    [[nodiscard]] bool noteOn(awe64::Chip& chip,std::uint8_t channel,
                              std::uint8_t key,std::uint8_t velocity) noexcept;
    [[nodiscard]] bool noteOff(awe64::Chip& chip,std::uint8_t channel,
                               std::uint8_t key) noexcept;
    [[nodiscard]] bool sustain(awe64::Chip& chip,std::uint8_t value) noexcept;
    // A source-valid MIDI ProgramChange alters only FUTURE bank zone
    // selection, never existing chip voices or their original releases.
    [[nodiscard]] bool programChange(std::uint8_t program) noexcept;
    // Explicit SF2 browser bank selection, NOT a conjectural Creative
    // Win95 MIDI CC0/CC32 hardware bank mapping. Select the first REAL
    // PHDR preset in that bank when its old program is absent, atomically.
    // Existing chip voices remain assigned to their previous samples.
    [[nodiscard]] bool selectExactSf2Bank(std::uint16_t bank) noexcept;
    [[nodiscard]] std::uint16_t currentBank() const noexcept {
        return bank_number_;
    }
    // Original Creative logical MIDI CC state used by NEXT NoteOn only.
    // Already sounding chip registers are not rewritten speculatively.
    [[nodiscard]] bool controller(std::uint8_t number,
                                  std::uint8_t value) noexcept;
    [[nodiscard]] std::uint8_t currentProgram() const noexcept {
        return program_;
    }
    // Project recall affects ONLY future MIDI note compilation. Existing
    // voices are never retriggered/released, and a missing real PHDR row
    // rejects the whole selection before changing anything.
    [[nodiscard]] bool restoreSelection(
        std::uint16_t bank,std::uint8_t program,
        std::uint8_t cc7,std::uint8_t cc10,std::uint8_t cc11) noexcept;
    [[nodiscard]] std::uint8_t currentCc7() const noexcept {
        return midi_.channel(channel_).cc7_volume;
    }
    [[nodiscard]] std::uint8_t currentCc10() const noexcept {
        return midi_.channel(channel_).cc10_pan;
    }
    [[nodiscard]] std::uint8_t currentCc11() const noexcept {
        return midi_.channel(channel_).cc11_expression;
    }
    // Exact PHDR inventory; no 77->128 padding and no invented preset.
    // Valid from setActive()/loaded bank; stable for browser/Prev/Next.
    [[nodiscard]] std::size_t presetCount() const noexcept {return presets_.size();}
    // PHDR index is the real SoundFont row, NOT bank*128+program.
    // All three entry points use one exact preset path so Browser,
    // Prev/Next, automation and VST3 recall cannot disagree.
    [[nodiscard]] bool selectPresetIndex(std::size_t index) noexcept;
    [[nodiscard]] bool stepPreset(int direction) noexcept;
    [[nodiscard]] std::size_t currentPresetIndex() const noexcept;
    [[nodiscard]] const awe64::creative::Sf2PresetIdentity* presetAt(
        std::size_t index) const noexcept {
        return index<presets_.size()?&presets_[index]:nullptr;
    }
    [[nodiscard]] std::size_t trackedVoices() const noexcept {
        return ownership_.trackedPhysicalVoices();
    }
    // Opt-in C4/D4 differential only. The hardware chip is rolled back
    // separately after speculative NoteOn/Off, but the original VXD
    // voice table, source ownership ledger and key-held map also change.
    // Capture these mutable bytes without heap allocation in process().
    // A mismatch MUST restore both chip and original VXD MIDI state.
    [[nodiscard]] bool beginNativeNoteCheckpoint() noexcept;
    void commitNativeNoteCheckpoint() noexcept;
    void rollbackNativeNoteCheckpoint() noexcept;
    [[nodiscard]] bool nativeNoteCheckpointActive() const noexcept {
        return native_checkpoint_active_;
    }
private:
    // Initial genuine DRY guest image validation. device+0x06 is the
    // physical capacity (32), device+0x18 is a mutable voice counter.
    // Requires genuinely idle rows, independent source attestation
    // still comes from the private pack and SHA-gated tables.
    [[nodiscard]] static bool acceptsOriginalIdleDeviceState(
        std::span<const std::uint8_t> original_state) noexcept;
    // Test-only friend has no runtime entry point; uses synthetic state.
    friend struct P002PrivateRomBankCheckpointProbe;
    std::vector<std::uint8_t> bank_bytes_;
    awe64::creative::Sf2PresetInventory presets_{};
    std::array<std::uint8_t,awe64::creative::kOriginalVxdMinDeviceImage> state_{};
    awe64::creative::OriginalCreativeSf2DriverResources resources_{};
    std::array<std::uint16_t,15> negative_{};
    std::array<std::uint16_t,16> positive_{};
    awe64::creative::OriginalSf2RomBankVoiceOwnership ownership_{};
    awe64::creative::MidiState midi_{};
    std::array<bool,128> held_keys_{};
    struct NativeNoteCheckpoint {
        std::array<std::uint8_t,awe64::creative::kOriginalVxdMinDeviceImage> state{};
        awe64::creative::OriginalSf2RomBankVoiceOwnership ownership{};
        std::array<bool,128> held_keys{};
        awe64::creative::MidiState midi{};
        std::uint16_t bank_number=0u;
        std::uint8_t channel=0u, program=0u;
        bool ready=false;
    };
    NativeNoteCheckpoint native_checkpoint_{};
    bool native_checkpoint_active_=false;
    std::uint16_t bank_number_=0;
    std::uint8_t channel_=0, program_=0;
    bool ready_=false;
    bool ram_mode_=false;
    // These two coordinates are authorized ONLY for the SHA-pinned
    // owner SF2 original captured Win95 upload (83 samples).
    std::uint32_t source_ram_base_=0u;
    std::size_t source_ram_pool_words_=0u;
};
} // namespace awe64::vst3
