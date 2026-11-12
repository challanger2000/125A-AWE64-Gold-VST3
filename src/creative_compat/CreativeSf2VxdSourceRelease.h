#pragma once
// P002: real original SBAWE.VXD LCOD+0x6072/0x60e4 NoteOff against the
// source-proven 32-byte Win95 VXD voice-state record.
//
// This is NOT a guessed SoundFont release and NOT the group choke.
// In the original 0x20-byte voice state:
//  +0x00 WORD marker = (MIDI channel<<8)|key
//  NoteOff sustain gate lives at CHANNEL row +0x04, not voice row +0x04.
//  +0x06 DWORD retained source sample end (0 means no loop rewrite)
//  +0x0A DWORD updated on conditional release-loop rewrite
//  +0x15 BYTE *already source-encoded* modulation envelope release
//  +0x16 BYTE *already source-encoded* volume envelope release
// The chip's original CSL / PSST upper bytes are read from the real
// pinned EMU8000, not reconstructed or forced to zero.
// Caller independently authenticates source VXD/SF2 evidence. The encoded
// generator30/38 release bytes are checked against the NoteOn descriptor.
// A synthetic regression record does not establish original PCM parity.
#include "CreativeSf2VxdStateSnapshot.h"
#include "CreativeVxdNoteRelease.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
namespace awe64::creative {
// A NoteOff may use the source VXD's two already-encoded release BYTES
// only if they still agree with the independently verified NoteOn
// descriptor (generator30/38) for that exact physical voice. Read-only;
// no substitute release envelope or per-preset rates are fabricated.
struct OriginalSf2VerifiedReleaseRates {
    std::uint8_t modulation=0;
    std::uint8_t volume=0;
};
[[nodiscard]] std::optional<OriginalSf2VerifiedReleaseRates>
candidateOriginalSf2VerifiedReleaseRates(
    std::span<const std::uint8_t> original_32byte_voice_record,
    const OriginalSf2VxdSnapshotVoice& verified_noteon_voice) noexcept;

struct OriginalSf2SourceNoteReleaseResult {
    bool deferred_by_sustain=false;
    std::size_t voice_count=0;
    std::size_t typed_port_writes=0;
};
[[nodiscard]] std::optional<OriginalSf2SourceNoteReleaseResult>
applyOriginalSf2MidiNoteOffFromVxdState(
    awe64::Chip& chip,
    std::span<std::uint8_t> verified_mutable_vxd_state,
    std::span<const OriginalSf2VxdSnapshotVoice> original_assigned_voices,
    std::uint8_t original_midi_channel,
    std::uint8_t played_key,
    bool independently_verified_original_voice_records) noexcept;
struct OriginalSf2SourceCc64Result {
    bool sustain_enabled=false;
    std::size_t released_deferred_voices=0;
    std::size_t typed_port_writes=0;
};
// Genuine original VXD LCOD+0x656D MIDI CC64 driver path:
// CC64 >=64 -> set channel-state+0x04 to 1, zero voice port writes.
// CC64 <64 -> set channel-state+0x04 to 0, scan the physical voice
// rows for markers with note low BYTE 0xFF and HIGH BYTE == channel,
// then call the *same actual source* LCOD+0x60E4 release on each.
// The public function is bounded to the separately source-verified
// assigned voices supplied by the active P002 diagnostic/allocator;
// complete 30-voice runtime scan/stealing is not claimed.
[[nodiscard]] std::optional<OriginalSf2SourceCc64Result>
applyOriginalSf2MidiCc64FromVxdState(
    awe64::Chip& chip,
    std::span<std::uint8_t> verified_mutable_vxd_state,
    std::span<const OriginalSf2VxdSnapshotVoice> original_assigned_voices,
    std::uint8_t original_midi_channel,
    std::uint8_t controller_value_0_to_127,
    bool independently_verified_original_voice_records) noexcept;
} // namespace awe64::creative
