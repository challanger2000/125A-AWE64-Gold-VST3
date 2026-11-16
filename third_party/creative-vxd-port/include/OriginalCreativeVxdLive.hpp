#pragma once
// Clean-room C++23 reconstruction of selected Creative SBAWE.VXD algorithms.
// Original Win95 VXD code/data is NOT embedded. This is a portable MIDI/EMU8000
// control fragment, NOT a complete Windows95 driver or whole VST3 instrument.
#include <array>
#include <cstdint>
#include <span>

namespace awe64::original_vxd {
constexpr std::size_t physical_voices = 32;
constexpr std::uint8_t no_voice = 0xff;

// Caller supplies the device+0x42 voice records read from its OWN native state.
// Offsets are grounded in authentic LCOD+0x3FE8 / +0x416C: record stride 0x20.
struct VoiceRecord {
    std::uint16_t key_marker = 0x0ffe;    // record+0x00
    std::uint32_t sample_end = 0;         // record+0x0a: cached end for allocator
    std::uint32_t release_sample_end = 0; // record+0x06: sample cursor endpoint
    std::uint8_t modulation_release = 0; // record+0x15
    std::uint8_t volume_release = 0;     // record+0x16
    bool blocked = false;                // record+0x17
    bool pair_penalty = false;           // record+0x1a bit0
    std::uint16_t original_pitch_base = 0; // source row+0x0e, unsigned
    bool original_pitch_base_verified = false;
    bool pan_state_verified = false; // original row +0x12,+0x18,+0x19
    std::uint8_t original_pan_base = 0; // source row+0x12
    std::uint8_t pan_target = 0; // row+0x18
    std::uint8_t pan_current = 0; // row+0x19
    bool volume_terms_verified = false;
    std::uint8_t original_velocity_volume_level = 0; // original voice+0x02 table lookup
    std::int16_t original_amplitude_word = 0; // original row+0x04
    bool original_modulation_word_verified = false;
    std::int8_t original_modulation_word_signed = 0; // original row+0x14
};
struct DeviceState {
    std::uint16_t physical_capacity = 32; // device+0x06
    std::uint32_t mode_count = 32;        // device+0x18 (MUTABLE)
    bool original_pan_device_mode_verified = false;
    std::uint16_t original_pan_device_mode = 0; // original device+0x12
    std::array<VoiceRecord, physical_voices> voices{};
    // Original per-MIDI-channel row device+0x442 + channel*0x24, +0x04.
    std::array<bool,16> channel_sustain{};
};

// The exact port semantics are preserved: E22 is 8-bit index,
// A20 16-bit DATA1, 620 32-bit DATA0, A20 32-bit DATA1.
// For 86Box's word-based host, read32/write32 is two ordered 16-bit
// callbacks at port and port+2. Callbacks must be RT-safe/nonthrowing.
// Original stereo-allocator helper CALLS, not synthetic CPU timestamps.
enum class StereoHelperKind : std::uint8_t { Vtft, Dcysusv, Ccca, Release };
struct StereoHelperEvent {
    StereoHelperKind kind;
    std::uint8_t voice;
};
struct PortIo {
    void* opaque = nullptr;
    bool (*out8)(void*, std::uint16_t, std::uint8_t) noexcept = nullptr;
    bool (*out16)(void*, std::uint16_t, std::uint16_t) noexcept = nullptr;
    bool (*in16)(void*, std::uint16_t, std::uint16_t&) noexcept = nullptr;
    bool (*in32)(void*, std::uint16_t, std::uint32_t&) noexcept = nullptr;
    bool (*out32)(void*, std::uint16_t, std::uint32_t) noexcept = nullptr;
    // Optional zero-heap, no-throw allocator observer. Disabled by default.
    void* stereo_helper_opaque = nullptr;
    void (*stereo_helper_observer)(void*, StereoHelperEvent) noexcept = nullptr;
};

enum class Status : std::uint8_t {
    Found, NoCandidate, MissingSource, PortError
};
struct Allocation {
    Status status = Status::MissingSource;
    std::uint8_t first = no_voice;
    std::uint8_t second = no_voice; // if paired, first+1
    std::uint32_t priority = 0;
    bool immediate = false;
    std::uint32_t io_reads = 0;
    std::uint32_t io_writes = 0; // includes selector writes
    std::uint32_t vtft_reads = 0;
    std::uint32_t dcysusv_reads = 0;
    std::uint32_t ccca_reads = 0;
    std::uint32_t completed_release_writes = 0;
    [[nodiscard]] constexpr std::uint32_t physical_port_callbacks() const noexcept {
        return 3u*vtft_reads + 2u*dcysusv_reads +
               3u*ccca_reads + 2u*completed_release_writes;
    }
};

// Original LCOD+0x3EA1: three interleaved physical lanes, prioritizes
// idle/sample-finished voices, genuine live VTFT/DCYSUSV/CCCA reads;
// exactly one original DCYSUSV stop/reset for the chosen physical voice.
// Uses the source device's capacity (1..32); it does NOT secretly hardcode 30.
[[nodiscard]] Allocation allocate_mono_mode0(DeviceState const&, PortIo) noexcept;

// Original LCOD+0x6072 -> +0x60E4 MIDI NoteOff and release body.
// A source 0x80 NoteOff or velocity-zero 0x90 NoteOn calls this same path.
// Uses actual channel sustain, physical key/channel state and the paired
// 32-bit CSL/PSST register transactions, preserving hardware upper bytes.
struct ReleaseResult {
    Status status = Status::MissingSource;
    std::uint32_t matched = 0;
    std::uint32_t released = 0;
    std::uint32_t deferred = 0;
    std::uint32_t io_reads = 0;
    std::uint32_t io_writes = 0;
};
[[nodiscard]] ReleaseResult release_note(DeviceState&, std::uint8_t channel,
                                         std::uint8_t key, PortIo) noexcept;
// Original CC64 +0x656D: value >=64 latches sustain; value <64 releases
// all matching deferred (channel:FF) voice rows via the SAME +0x60E4 body.
[[nodiscard]] ReleaseResult controller_sustain(DeviceState&,std::uint8_t channel,
                                               std::uint8_t cc_value,PortIo) noexcept;
// Original LCOD+0x6C82: CC120 / CC123 share the *actual* +0x60E4
// release body. CC120 ignores sustain, CC123 defers while pedal down.
[[nodiscard]] ReleaseResult original_channel_all_notes_off(
    DeviceState&,std::uint8_t channel,bool observe_sustain,PortIo) noexcept;
// Original LCOD+0x3FE8, including REAL lazy VTFT/DCYSUSV/CCCA reads,
// reverse scan, cheapest adjacent voice pair, original E22/A20 releases.
[[nodiscard]] Allocation allocate_pair_mode1(DeviceState const&, PortIo) noexcept;
// Original LCOD+0x416C, packed low 8 bits=begin, remaining=exclusive end.
// Includes ascending scan, immediate idle/sample-end exits and port release.
[[nodiscard]] Allocation allocate_range_mode2(DeviceState const&, std::uint32_t packed_range, PortIo) noexcept;

// No memory allocations; supports physical 30/31 when original pair search
// selected them, and does NOT silently impose the mono-mode budget of 30.
} // namespace awe64::original_vxd
