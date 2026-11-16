#pragma once
// Portable clean-room reconstruction of the 28 DATA values from the original
// Creative SBAWE.VXD SF2 NoteOn 56-OUT main sequence (P002/P003 verified
// LCOD+0x5BE4..0x6021). Inputs MUST be post-source-encoded 148-byte original
// voice descriptor and authenticated physical sample/voice/channel state.
// Not a SoundFont generator default converter; ROM and verified RAM only.
#include "OriginalCreativeMidiEngine.hpp"
#include <cstdint>

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
};
// Does not send any hardware writes or allocate. The stereo worklist's
// sampleType 2/4 has to be normalized by the authentic Creative descriptor
// builder FIRST; sampleType 1 is the verified post-start format here.
[[nodiscard]] bool compile_original_sf2_main(SourceVoice const&,
    std::uint8_t physical, std::uint8_t midi_key,
    OriginalVoiceStartSources const&, OriginalVoiceProgram&) noexcept;
} // namespace awe64::original_vxd
