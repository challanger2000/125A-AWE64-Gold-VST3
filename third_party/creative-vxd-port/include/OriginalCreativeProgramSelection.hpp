#pragma once
// SBAWE.VXD LCOD+0x6D05 -> +0x6E71: melodic Program Change bank fallback.
// Does not fabricate a SoundFont parser, drum-channel resolver, or sound data.
#include <cstdint>

namespace awe64::original_vxd {
struct OriginalProgramBankSelection {
    bool found=false;
    std::uint8_t selected_bank=0;
    std::uint8_t fallback_pass=0;
};
using OriginalProgramBankContains = bool (*)(const void*,std::uint8_t bank,
                                             std::uint8_t program) noexcept;
// Only call with a validated original program directory: the callback answers
// whether a bank/program actually exists, not whether it matches a MIDI key.
[[nodiscard]] OriginalProgramBankSelection original_melodic_program_bank(
    std::uint8_t requested_bank,std::uint8_t program,
    const void* opaque,OriginalProgramBankContains contains) noexcept;
} // namespace awe64::original_vxd
