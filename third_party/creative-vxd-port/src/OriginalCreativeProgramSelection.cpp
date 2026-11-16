#include "OriginalCreativeProgramSelection.hpp"
#include <array>

namespace awe64::original_vxd {
OriginalProgramBankSelection original_melodic_program_bank(
    const std::uint8_t requested_bank,const std::uint8_t program,
    const void* opaque,const OriginalProgramBankContains contains) noexcept {
    OriginalProgramBankSelection result{};
    if(!contains || program>127u)return result;
    // Genuine original +0x6E71: requested 0 starts at bank 128; any other
    // requested bank gets its own first pass. On a miss, source searches
    // bank 128, then bank 0. This is program selection, BEFORE key zoning.
    const std::array<std::uint8_t,3> attempts{
        static_cast<std::uint8_t>(requested_bank==0u?128u:requested_bank),
        128u,0u};
    std::uint8_t pass=0u;
    std::uint8_t previous=255u;
    for(const auto bank:attempts) {
        if(bank==previous)continue;
        if(contains(opaque,bank,program)) {
            result.found=true;
            result.selected_bank=bank;
            result.fallback_pass=pass;
            return result;
        }
        previous=bank;
        ++pass;
    }
    return result;
}
} // namespace awe64::original_vxd
