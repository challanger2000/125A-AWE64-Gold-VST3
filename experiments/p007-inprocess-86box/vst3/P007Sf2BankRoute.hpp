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
    // The GUI deliberately retains GM ROM on logical bank 0 and shows
    // the single-bank loaded SF2's 1..128 user patches on logical BANK 1.
    // Creative WIN95 USER bank slot #1 is selected with CC0=1, CC32=0.
    // NEVER hijack ROM bank 0 after SF2 loading, and never claim the
    // user bank before original AWEMAN32 has acknowledged its load.
    return originalGuestSf2Loaded && requested==1u
        ?Selection{1u,0u}:Selection{0u,requested};
}
} // namespace p007::sf2_bank
