#pragma once
// Bounded, non-owning view over original-driver-verified *encoded* SF2
// voice zones. The original bank parser/zone converter is NOT replaced here.
// This adapter connects that already-authenticated source to the fully
// portable MIDI engine and 28-register compiler without fixture callbacks.
#include "OriginalCreativeSf2MainCompiler.hpp"
#include "OriginalCreativeProgramBankMap.hpp"
#include <cstdint>
#include <span>

namespace awe64::original_vxd {
struct VerifiedSf2Zone {
    bool original_zone_verified=false;
    std::uint8_t bank=0, program=0;
    // Original +0x6D05 chooses a specific 26-byte preset ROW within a
    // concrete device bank SLOT. Equal bank/program bytes may occur in
    // multiple loaded SoundFonts; never merge their zones accidentally.
    // Required when the authenticated 288-slot map is enabled.
    bool original_preset_identity_verified=false;
    std::uint16_t original_bank_slot=0;
    std::uint16_t original_preset_row=0;
    std::uint8_t key_low=0, key_high=127;
    std::uint8_t velocity_low=1, velocity_high=127;
    SourceVoice voice{};
    OriginalVoiceStartSources original_state{};
};

struct VerifiedSf2Bank {
    // Lifetime must extend across every MIDI callback. No ownership,
    // copies or allocations. Multiple matching zones are legitimate.
    std::span<const VerifiedSf2Zone> zones{};
    // Only enable when the supplied zone directory is genuinely validated as
    // the original Creative melodic program directory. This follows VXD
    // +0x6E71 bank fallback; drum channel 9 uses different source logic.
    bool original_melodic_program_fallback_verified=false;
    // Optional fully source-backed 288-slot original device program table.
    // When present, its original row/slot lookup takes precedence over the
    // earlier coarse boolean bank->program existence callback.
    OriginalProgramBankMap original_program_map{};
};

[[nodiscard]] SoundFontSource original_verified_sf2_bank_source(
    VerifiedSf2Bank& bank) noexcept;
} // namespace awe64::original_vxd
