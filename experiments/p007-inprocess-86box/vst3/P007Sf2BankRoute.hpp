#pragma once
// P007AU: use the ORIGINAL 1996 Creative Win95 bank convention.
// Original WINAWE32.C explicitly sends CC0=1 to select user bank #1,
// not CC32=1. This is a driver command, not a replacement synthesizer.
// The AWE_LOAD_USER_BANK API success alone is not audio parity proof.
#include <cstdint>
namespace p007::sf2_bank {
struct Selection {
    std::uint8_t cc0{},cc32{};
};
inline Selection route(std::uint8_t requested,bool originalGuestSf2Loaded) noexcept {
    // Logical SF2 bank 0's presets live in ORIGINAL Creative user bank 1;
    // ROM and all nonzero previously selectable guest banks are unchanged.
    return originalGuestSf2Loaded && requested==0u
        ?Selection{1u,0u}:Selection{0u,requested};
}
} // namespace p007::sf2_bank
