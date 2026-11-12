#pragma once
// P002: real original SBAWE.VXD LCOD+0x6072/0x60e4 NoteOff against the
// source-proven 32-byte Win95 VXD voice-state record.
//
// This is NOT a guessed SoundFont release and NOT the group choke.
// In the original 0x20-byte voice state:
//  +0x00 WORD marker = (MIDI channel<<8)|key
//  +0x04 BYTE held by sustain; note marker becomes channel:FF, no HW IO
//  +0x06 DWORD retained source sample end (0 means no loop rewrite)
//  +0x0A DWORD updated on conditional release-loop rewrite
//  +0x15 BYTE *already source-encoded* modulation envelope release
//  +0x16 BYTE *already source-encoded* volume envelope release
// The chip's original CSL / PSST upper bytes are read from the real
// pinned EMU8000, not reconstructed or forced to zero.
// Caller guarantees the source voice-state record itself is original
// VXD evidence; the source of those rates from NoteOn is STILL open.
// A synthetic regression record does not establish original PCM parity.
#include "CreativeSf2VxdStateSnapshot.h"
#include "CreativeVxdNoteRelease.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
namespace awe64::creative {
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
} // namespace awe64::creative
