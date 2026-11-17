#pragma once
// Portable clean-room reconstruction of the 28 DATA values from the original
// Creative SBAWE.VXD SF2 NoteOn 56-OUT main sequence (P002/P003 verified
// LCOD+0x5BE4..0x6021). Inputs MUST be post-source-encoded 148-byte original
// voice descriptor and authenticated physical sample/voice/channel state.
// Not a SoundFont generator default converter; ROM and verified RAM only.
#include "OriginalCreativeMidiEngine.hpp"
#include <cstdint>
#include <optional>

namespace awe64::original_vxd {
struct OriginalVoiceStartSources {
    bool original_descriptor_verified=false;
    bool original_device_state_verified=false;
    bool original_sample_placement_verified=false;
    bool original_ram_sample=false;
    std::int16_t original_channel_tune_1_cents=0;
    std::int16_t original_channel_tune_2_cents=0;
    std::uint8_t original_lfo1_pitch_device_0a=0;
    std::uint8_t original_lfo1_pitch_device_0b=0;
    std::uint16_t original_voice_volume_target=0;
    std::uint16_t original_pitch_target=0;
    // Opt-in to the actual LCOD channel/target calculations. Existing
    // externally verified literal targets remain usable when false.
    bool original_derive_pitch_target_from_channel=false;
    std::int16_t original_channel_pitch_tune=0;
    std::int32_t original_channel_bend_delta=0;
    bool original_derive_volume_target_from_attenuation=false;
    bool original_volume_target_tables_verified=false;
    std::span<const std::uint16_t> original_volume_positive16{};
    std::span<const std::uint16_t> original_volume_negative15{};
};
// LCOD+0x56CE..0x5753 baseline stored at physical voice+0x0E,
// before channel pitch-bend/tune are applied at LCOD+0x5B50.
[[nodiscard]] std::optional<std::uint16_t> original_sf2_initial_pitch(
    Descriptor const&,std::uint8_t key,std::int16_t tune1,
    std::int16_t tune2) noexcept;
// Does not send any hardware writes or allocate. The stereo worklist's
// sampleType 2/4 has to be normalized by the authentic Creative descriptor
// builder FIRST; sampleType 1 is the verified post-start format here.
[[nodiscard]] bool compile_original_sf2_main(SourceVoice const&,
    std::uint8_t physical, std::uint8_t midi_key,
    OriginalVoiceStartSources const&, OriginalVoiceProgram&) noexcept;
} // namespace awe64::original_vxd
