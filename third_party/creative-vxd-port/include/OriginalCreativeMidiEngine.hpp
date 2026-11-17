#pragma once
// Portable, heap-free MIDI-to-EMU8000 orchestration. The 28 DATA operations
// per voice are sourced from the original driver-derived P002/P003 composer,
// not fitted to an audio clip or manufactured from unverified SoundFont data.
// The source-verified P002 composer must supply the actual 28 data values.
#include "OriginalCreativeStereoWorklist.hpp"
#include "OriginalCreativeProgramBankMap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace awe64::original_vxd {
inline constexpr std::size_t max_source_note_layers=6;
inline constexpr std::size_t original_note_main_data_registers=28;

struct MidiChannel {
    bool original_channel_is_percussion=false; // resolve-only MIDI channel 9 marker
    std::uint8_t bank_msb=0, bank_lsb=0, program=0;
    // Original +0x6D05 stores selected patch state in three WORDs at
    // channel+0x00/+0x02/+0x18 and the resolved bank byte +0x1B.
    // Subsequent CC0 changes the requested bank without reselecting a
    // loaded program until the NEXT Program Change.
    bool original_program_selection_valid=false;
    std::uint16_t original_selected_bank_slot=0;
    std::uint16_t original_selected_preset_row=0;
    std::uint8_t original_selected_program=0;
    std::uint8_t original_selected_bank=0;
    std::uint8_t volume=100, expression=127, pan=64;
    std::uint8_t chorus=0, reverb=0;
    std::uint8_t modulation=0, original_modulation_bucket=0;
    bool original_modulation_byte_44d_verified=false;
    std::uint8_t original_modulation_byte_44d=0;
    std::uint16_t pitch_bend=8192;
    std::uint8_t original_bend_range=0; // source channel+0x0d; 0=>default2
    std::int16_t original_pitch_tune=0; // source channel+0x0e
    std::int32_t original_bend_delta=0; // source channel+0x14
    std::uint16_t original_rpn_mode=0; // source channel+0x45E (0x100=RPN)
    std::uint16_t original_rpn_number=0; // source channel+0x460
    std::uint16_t original_rpn_data_lsb=0; // source channel+0x464
    std::uint16_t original_rpn_aux_462=0; // original CC121 clears +0x462
    std::int16_t original_fine_tune_cents=0; // source channel+0x452
    std::int16_t original_coarse_tune_cents=0; // source channel+0x454
    bool original_fine_tune_verified=false, original_coarse_tune_verified=false;
};

struct SourceVoice {
    Descriptor descriptor{};       // SF2 worklist metadata (type=2/4, or mono)
    Descriptor note_start_descriptor{}; // independently encoded source type=1
    bool note_start_descriptor_verified=false;
    std::uint32_t release_end=0; // original 24-bit sample-end endpoint
    std::uint16_t original_pitch_base=0; // original device voice row +0x0e
    bool original_pitch_base_verified=false;
    bool pan_state_verified=false;
    std::uint8_t original_pan_base=0, pan_target=0, pan_current=0;
    bool volume_terms_verified=false;
    std::uint8_t original_velocity_volume_level=0; // original x86 lookup of voice+2
    std::int16_t original_amplitude_word=0; // original voice row+0x04
    bool original_modulation_word_verified=false;
    std::int8_t original_modulation_word_signed=0; // original row+0x14
    bool source_verified=false;
    std::uint16_t source_slot=0xffffu; // source adapter index, not SF2 sampleID
    bool original_fine_tune_override=false, original_coarse_tune_override=false;
    std::int16_t original_fine_tune_cents=0, original_coarse_tune_cents=0;
};

// Fixed original VXD LCOD+0x5BE4..0x6021 main register order (28 x E22+DATA).
// The selected physical voice is inserted into every E22 selector by this
// module; no caller-supplied arbitrary hardware selectors are accepted.
struct OriginalVoiceProgram {
    std::array<std::uint32_t,original_note_main_data_registers> data{};
    bool source_verified=false;
};

struct SoundFontSource {
    void* opaque=nullptr;
    // Must resolve from the actual bank/program/key/velocity and original
    // generator engine. No work on the audio thread that allocates memory.
    bool (*resolve)(void*,MidiChannel const&,std::uint8_t key,
                    std::uint8_t velocity,
                    std::array<SourceVoice,max_source_note_layers>&,
                    std::uint8_t& count) noexcept=nullptr;
    // Must compute original 28 DATA values from the exact encoded descriptor
    // and independently verified channel/device/voice state. Pure function:
    // no chip writes and no allocation. Unknown inputs => false.
    bool (*compile)(void*,SourceVoice const&,std::uint8_t physical,
                    std::uint8_t key,OriginalVoiceProgram&) noexcept=nullptr;
    // Original LCOD+0x6D05 selects its bank/preset HERE, at Program
    // Change (not inside later NoteOn). Pure, no I/O, heap or callback
    // into Windows95. Unknown original table => false.
    bool (*select_program)(void*,MidiChannel const&,std::uint8_t program,
                           OriginalProgramSelection&) noexcept=nullptr;
};

enum class MidiResult : std::uint8_t {
    Applied, Incomplete, IgnoredRealtime, Unsupported, Invalid,
    MissingSource, NoVoice, PortError, FaultLatched
};

struct MidiEngine {
    // Immutable source resources and actual owner VXD tables are injected
    // non-owningly; tables must outlive MIDI processing.
    struct VolumeSource {
        bool verified=false;
        std::uint8_t const* cc7_table_128=nullptr;
        std::uint8_t const* cc11_table_128=nullptr;
        std::uint16_t device_additive=0;
        bool device_additive_verified=false;
        bool format1_verified=false;
        bool format1=false;
    } volume_source{};
    // Original LCOD+0x6C68 invokes an optional device+0xB5A callback.
    // There is no justified no-op unless a private source snapshot proves
    // that pointer NULL; otherwise CC121 returns MissingSource.
    bool original_reset_callback_absent_verified=false;
    DeviceState device{};
    std::array<MidiChannel,16> channel{};
    // MIDI stream state (serial input), separate from a VST3 host's already
    // parsed per-event MIDI messages.
    std::uint8_t running_status=0;
    std::uint8_t message_status=0;
    std::uint8_t data_bytes[2]{0,0};
    std::uint8_t data_count=0, data_need=0;
    std::uint8_t system_remaining=0;
    bool sysex=false, fault=false;
};

// Single event. The corresponding exact note-on register program MUST exist
// before voice state is committed; any port error latches fault for the
// lifetime of the instance (no fake rollback of real hardware writes).
[[nodiscard]] MidiResult dispatch_midi(MidiEngine&,std::uint8_t status,
                                       std::uint8_t d1,std::uint8_t d2,
                                       PortIo,SoundFontSource) noexcept;
// Byte stream parser: channel running status; realtime messages do not
// interrupt a partial channel message. No invented SysEx effects.
[[nodiscard]] MidiResult feed_midi_byte(MidiEngine&,std::uint8_t byte,
                                       PortIo,SoundFontSource) noexcept;

// Direct callback for existing 125A Chip's real ports. Each voice uses the
// exact original sequence; no synthetic ADSR, resampling or alternate DSP.
[[nodiscard]] bool write_original_note_main(std::uint8_t physical,
                                            OriginalVoiceProgram const&,
                                            PortIo) noexcept;
} // namespace awe64::original_vxd
