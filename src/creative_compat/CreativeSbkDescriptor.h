#pragma once
// P002 read-only Creative 1MGM/SYNTHGM.SBK descriptor selection.
// No sample loading, allocation, RT IO, or full Creative-driver claim.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace awe64::creative {
struct SbkZoneDescriptor {
    std::uint16_t bank = 0;
    std::uint8_t program = 0;
    std::uint8_t selected_key = 0;
    std::uint8_t selected_velocity = 0;
    std::uint16_t instrument_index = 0;
    std::uint16_t instrument_zone_bag = 0;
    std::uint8_t key_low = 0, key_high = 0;
    std::uint16_t sample_id_raw = 0; // unvalidated sample address, P003
    std::uint8_t effective_raw_cutoff = 0;
    std::uint8_t chip_cutoff_candidate = 0; // measured 1MGM mapping 2*raw
    std::optional<std::uint16_t> global_raw_cutoff;
    std::optional<std::uint16_t> zone_raw_cutoff;
    std::optional<std::uint16_t> preset_attenuation_raw;
    std::optional<std::uint16_t> global_attack_vol_env_raw;
};
// Preflight off the audio thread. Rejects incomplete/ambiguous/unsupported
// metadata instead of substituting a GM-program cutoff or guessed layer.
// Original Creative 48-byte shdr records are required (not 46-byte SF2).
[[nodiscard]] std::optional<SbkZoneDescriptor> resolveCreativeSbkZone(
    std::span<const std::uint8_t> sbk,
    std::uint16_t bank, std::uint8_t program, std::uint8_t key,
    std::uint8_t velocity) noexcept;
} // namespace awe64::creative
