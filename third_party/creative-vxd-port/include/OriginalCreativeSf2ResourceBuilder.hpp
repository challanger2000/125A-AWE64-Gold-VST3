#pragma once
// Source-driven Creative SF2 descriptor pipeline. Structural SF2 data comes
// from OriginalCreativeSf2SourceReader. Original Creative default generator
// templates and sample-rate pitch lookup must be separately authenticated.
// Runtime uses no Windows 95, private driver bytes, heap or floating point.
// The 1996 driver's full resource loader has NOT yet been proven equivalent.
#include "OriginalCreativeSf2SourceReader.hpp"
#include "OriginalCreativeSf2RamUpload.hpp"
#include "OriginalCreativeVerifiedSf2Bank.hpp"
#include "OriginalCreativeVolumeController.hpp"
#include <array>
#include <cstdint>
#include <span>

namespace awe64::original_vxd {
struct OriginalSf2SourceTables {
    bool original_generator_templates_verified=false;
    bool original_pitch_tables_verified=false;
    std::array<std::int16_t,59> preset_template{};
    std::array<std::int16_t,59> instrument_template{};
    std::array<std::uint32_t,128> pitch_base{};
    std::array<std::int8_t,128> pitch_slope{};
    // Actual original Creative sample RAM placement, not an SF2-file
    // offset guessed from the file. Needed for all RAM samples.
    bool original_sample_ram_placement_verified=false;
    std::uint32_t ram_base_word=0;
    // Stereo type2/4 -> type1 note-start normalization independently proven.
    bool original_stereo_descriptor_conversion_verified=false;
};
struct OriginalSf2SourceChannelState {
    bool original_note_context_verified=false;
    bool original_attenuation_verified=false;
    std::uint16_t original_encoded_attenuation=0;
    std::uint8_t original_channel_pan=64;
    std::uint8_t original_channel_chorus=0;
    std::uint8_t original_channel_reverb=0;
    std::uint8_t original_device_effects_flag=0;
    OriginalVoiceStartSources original_voice_start{};
    bool original_voice_pitch_base_verified=false;
    std::uint16_t original_voice_pitch_base=0;
    bool original_program_identity_verified=false;
    std::uint16_t original_program_bank_slot=0;
    std::uint16_t original_program_preset_row=0;
    // Optional autonomous LCOD+0x5753 NoteOn attenuation. If supplied,
    // the original velocity/CC tables and merged generator48 determine
    // gen19 for this note. No per-key/velocity attenuation table/cache.
    // An additionally supplied verified value must agree exactly.
    OriginalNoteVolumeResources const* original_note_volume_source=nullptr;
    std::uint8_t original_channel_volume=100;
    std::uint8_t original_channel_expression=127;
    // An authenticated loader can map a physical VXD row to a different
    // source PHDR ordinal and logical bank/program. Never infer this
    // association from equal bytes or the directory child index.
    bool original_source_preset_binding_verified=false;
    std::uint16_t original_source_preset_row=0;
    std::uint32_t original_descriptor_group=0;
};
enum class Sf2BuildStatus:std::uint8_t {
    Ok,Unverified,Unsupported,Malformed,TooManyLayers
};
struct OriginalSf2PreparedBank {
    Sf2BuildStatus status=Sf2BuildStatus::Malformed;
    std::array<VerifiedSf2Zone,sf2_max_selected_layers> zones{};
    std::uint8_t count=0;
};
// Two descriptors are produced per layer:
//  - source voice descriptor: original sample class 1/2/4 and link for the
//    original stereo worklist;
//  - encoded note-start descriptor: sampleType=1 and all 28 hardware
//    register data fields prepared from authenticated Creative resources.
// Note-dependent resources (velocity/CCs) must be verified for this EXACT
// selection. A missing original table/state returns Unverified, not zero.
// Bind a successfully *completed* authentic RAM bank upload to the
// note-descriptor source. Its sample base is allocation_start+14, NOT
// allocation_start: the original x86 PCOD writer inserts a 14-word prelude.
// Any invalid/partial result invalidates a previously loaded location,
// preventing a stale bank pointer from being used after hardware failure.
[[nodiscard]] bool bind_original_creative_uploaded_ram_bank(
    OriginalSf2SourceTables& resources,
    const OriginalCreativeRamUploadResult& uploaded) noexcept;
[[nodiscard]] OriginalSf2PreparedBank prepare_original_sf2_zones(
    Sf2LayerSelection const& selected,
    OriginalSf2SourceTables const& original_tables,
    std::span<const OriginalSf2SourceChannelState> original_layer_states) noexcept;
} // namespace awe64::original_vxd
