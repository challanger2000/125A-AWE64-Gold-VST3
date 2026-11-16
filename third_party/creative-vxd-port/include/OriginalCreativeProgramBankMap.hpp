#pragma once
// Portable representation of the verified ORIGINAL Win95 Creative VXD
// program table at device+0x6CA. Each native row is a normalized view
// of the original 0x1A-byte record's WORD +0, BYTE +2 and BYTE +3;
// source sections/tables are provided separately, never invented here.
#include <cstdint>
#include <span>

namespace awe64::original_vxd {
inline constexpr std::size_t original_program_bank_slots=0x120u;
struct OriginalPresetTableRow {
    std::uint16_t original_source_marker=0xffffu; // 0/FFFF/valid nonzero
    std::uint8_t original_program_byte=0u;        // VXD entry +0x02
    std::uint8_t original_bank_byte=0u;           // VXD entry +0x03
};
struct OriginalProgramBankSlot {
    // Empty: missing bank pointer (original device+0x6CA[index] == 0).
    // Otherwise must end with an authenticated 0xFFFF sentinel.
    std::span<const OriginalPresetTableRow> rows{};
};
struct OriginalProgramBankMap {
    bool authentic_driver_records_verified=false;
    std::span<const OriginalProgramBankSlot> slots{}; // exactly 0x120
};
enum class ProgramSelectionStatus : std::uint8_t {
    Found, NoOriginalPreset, MissingOriginalSource
};
struct OriginalProgramSelection {
    ProgramSelectionStatus status=ProgramSelectionStatus::MissingOriginalSource;
    std::uint16_t device_bank_slot=0; // original channel row +0x18
    std::uint16_t original_preset_row=0; // original channel row +0x00
    std::uint16_t resolved_program=0; // original channel row +0x02
    std::uint8_t resolved_bank=0; // original channel row +0x1B
    std::uint8_t fallback_stage=0; // 0=requested, 1/2=source fallback
};
// Original LCOD+0x6E71 melodic path of +0x6D05, 16 MIDI channels except
// percussion channel 9. Searches ORIGINAL normalized VXD bank records;
// fallback stages: requested-bank, bank 128, bank 0, with special bank0
// starting at slot128. Does NOT synthesize a bank or any SF2 descriptor.
[[nodiscard]] OriginalProgramSelection select_original_melodic_program(
    OriginalProgramBankMap const&,std::uint8_t selected_bank,
    std::uint8_t program) noexcept;
// Original +0x6D55 percussion channel-9 bank/program allocator. Searches
// an ordered interval for the best MIDI drum-program byte <= requested|128,
// retaining the source's peculiar slot 128 -> 129 step and subsequent
// fallbacks to drum bank 128 and bank 0. It does not manufacture GM drums.
[[nodiscard]] OriginalProgramSelection select_original_percussion_program(
    OriginalProgramBankMap const&,std::uint8_t selected_bank,
    std::uint8_t program) noexcept;
} // namespace awe64::original_vxd
