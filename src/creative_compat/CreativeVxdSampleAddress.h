#pragma once
// P002: specific authentic SBAWE.VXD sample-address update cases 5 and 6.
// This is NOT generic SoundFont key-on, and is not a guessed loop policy.
#include "CreativeSbkLayers.h"
#include "CreativeVxdMonoVoiceRanking.h"
#include <array>
#include <cstdint>
#include <optional>
namespace awe64::creative {
enum class OriginalVxdSampleAddressCase : std::uint8_t {
    DriverCase5 = 5,  // VXD LCOD +0x1B4A; loop end +1 / loop start
    DriverCase6 = 6   // VXD LCOD +0x1C19; sample end +8 / sample end +4
};
// Original driver reads the CURRENT 32-bit PSST value first and preserves
// bits 31..24 (pan / auxiliary send). Caller MUST obtain actual hardware
// PSST, rather than assuming a default zero upper byte.
// Each case emits CSL FIRST (DATA0 index7) and PSST SECOND (DATA0 index6),
// using original E22 OUT AL then 620 OUT EAX for each pair.
[[nodiscard]] std::optional<std::array<OriginalWin95VxdPortWrite,4>>
candidateOriginalVxdSampleAddressUpdate(
    const SbkRomAddress& sample, std::uint8_t physical_voice,
    OriginalVxdSampleAddressCase driver_case,
    std::uint32_t actual_prior_psst) noexcept;

enum class OriginalVxdCccaStartPhase : std::uint8_t {
    StartWithoutFilterQ=0,
    StartWithEncodedFilterQ=1
};
// The private Creative VXD NOTE START at LCOD +0x5B81 first derives
// CCCA from ROM word-start -4 and writes it at +0x5F28. At +0x5F58
// it writes the same CCCA AGAIN with pre-encoded filter Q in bits 31..28.
// These are separate phases: other envelope registers occur BETWEEN.
// Caller supplies the original encoded voice filter-Q after original
// bank/controller conversion. No synthetic resonance approximation.
[[nodiscard]] std::optional<std::array<OriginalWin95VxdPortWrite,2>>
candidateOriginalVxdRomCccaStart(
    const SbkRomAddress& sample,std::uint8_t physical_voice,
    std::uint8_t encoded_original_filter_q,
    bool original_driver_filter_q_verified,
    OriginalVxdCccaStartPhase phase) noexcept;

struct OriginalVxdRomNoteStartMode {
    bool original_driver_mode_verified=false;
    // The genuine original VXD's already encoded record byte +0x6C.
    // Only mode bits 0 (loop) and 1 (retain release end marker)
    // are recovered for this code path.
    std::uint8_t encoded_mode_flags=0;
    // Raw already encoded original 0x94-byte record pan/chorus fields;
    // caller must not substitute generic uncalibrated SF2 quantities.
    std::uint8_t original_pan_byte=0;
    std::uint8_t original_chorus_send_byte=0;
};
struct OriginalVxdRomNoteStartGeometry {
    std::array<OriginalWin95VxdPortWrite,4> loop_address_ports{};
    std::uint32_t driver_retained_release_end=0;
};
// Private original SBAWE.VXD +0x5B81..+0x5BE4 and +0x5EEB..+0x5F1B:
// initial NoteOn loops and pan/chorus send. Unlike the separate source
// case5/6 update, initial NoteOn writes PSST FIRST, CSL SECOND.
// The CCCA filter Q writes occur later and remain separate.
[[nodiscard]] std::optional<OriginalVxdRomNoteStartGeometry>
candidateOriginalVxdInitialRomNoteGeometry(
    const SbkRomAddress& sample,std::uint8_t physical_voice,
    OriginalVxdRomNoteStartMode encoded_record) noexcept;

// Source-confirmed P002 original Windows-95 RAM SoundFont note geometry.
// One independently captured owner bank (83 samples, sample 82) proved
// CCCA=physical shdr.start-4, looping PSST=loopStart-1 and
// CSL=loopEnd-1 in all four original C4/D4 physical voices.
// This helper DOES NOT allocate RAM, decode SF2 generators, choose a key,
// guess pan/resonance/send values, or produce complete original NoteOn.
//
// Caller must have independently authenticated (a) the actual Creative
// chip RAM upload's physical address mapping and (b) the already encoded
// original VXD filter/pan/chorus bytes. Values are deliberate inputs,
// not new default DSP or unsupported global Creative-bank formula.
struct OriginalVxdRamNoteStartGeometry {
    std::array<OriginalWin95VxdPortWrite,2> plain_ccca{};
    std::array<OriginalWin95VxdPortWrite,2> filtered_ccca{};
    std::array<OriginalWin95VxdPortWrite,4> loop_address_ports{};
};
[[nodiscard]] std::optional<OriginalVxdRamNoteStartGeometry>
candidateOriginalVxdInitialVerifiedRamNoteGeometry(
    std::uint32_t physical_sample_start,
    std::uint32_t physical_sample_end,
    std::uint32_t physical_loop_start,
    std::uint32_t physical_loop_end,
    std::uint8_t physical_voice,
    std::uint8_t original_filter_q_encoded,
    std::uint8_t original_pan_byte,
    std::uint8_t original_chorus_byte,
    bool independently_verified_original_ram_placement,
    bool independently_verified_original_vxd_mode) noexcept;
} // namespace awe64::creative
