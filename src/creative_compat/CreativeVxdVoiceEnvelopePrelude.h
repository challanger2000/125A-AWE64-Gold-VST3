#pragma once
// Genuine original Win95 SBAWE.VXD NOTE START, FIRST seven voice register
// writes (LCOD +0x5BE4..+0x5CA4). This is a bounded hardware prelude only,
// not an SF generator conversion, not complete NoteOn/PCM acceptance.
#include "CreativeVxdMonoVoiceRanking.h"
#include <array>
#include <cstdint>
#include <optional>
namespace awe64::creative {
struct OriginalVxdEncodedVoiceEnvelopePrelude {
    bool from_actual_original_driver_record=false;
    // Original VXD 0x94-byte per-instrument allocated voice RECORD fields.
    // Values have already undergone its earlier generator arithmetic.
    std::uint16_t attack_hold_volume_low_byte_word=0; // [record+0x44]
    std::int16_t attack_hold_volume_high_word=0;      // [+0x46]
    std::int16_t lfo1_delay_word=0;                    // [+0x2A]
    std::uint16_t attack_hold_mod_low_byte_word=0;    // [+0x34]
    std::int16_t attack_hold_mod_high_word=0;         // [+0x36]
    std::uint8_t decay_sustain_mod_low_byte=0;        // [+0x38]
    std::int16_t decay_sustain_mod_high_word=0;       // [+0x3A]
    std::int16_t lfo2_delay_word=0;                    // [+0x2E]
};
// Original VXD resets 32-bit VTFT and CVCF both to 0000FFFF,
// then DATA2 index4 ATKHLDV, DATA2 index5 LFO1VAL,
// DATA2 index6 ATKHLD, DATA1 index7 DCYSUS, DATA2 index7 LFO2VAL.
// Total 14 typed OUT instructions, with all write widths exact.
// Neither note trigger nor sample CCCA/IP/IFATN/pan/FX is in this subset.
[[nodiscard]] std::optional<std::array<OriginalWin95VxdPortWrite,14>>
candidateOriginalVxdVoiceEnvelopePrelude(
    std::uint8_t physical_voice,
    const OriginalVxdEncodedVoiceEnvelopePrelude& record) noexcept;
// Real original SBAWE.VXD PCOD +0x3791 envelope-rate lookup from a
// PRE-CALIBRATED 16-bit VXD time word (128 threshold WORDs at LCOD +0x31A8).
// Physical input units are UNKNOWN: raw SF1/SBK generator34 is NOT input.
[[nodiscard]] std::uint8_t originalCreativePcodEnvelopeRateFromWord(
    std::uint16_t actual_original_driver_time_word) noexcept;
// Actual original Creative 1MGM PCOD generator dispatcher +0x37BA,
// resolved via the LE page-fixup jump table. Generator 26/34 use the
// DIFFERENT ATTACK helper in LCOD+0x74F2; 28/30/38 use PCOD+0x3791.
// This maps genuine SBK generator WORDS in the original SF1/1MGM path,
// not unverified generalized SF2 timecents. Other generators fail closed.
[[nodiscard]] std::uint8_t originalCreativePcodAttackRateFromWord(
    std::uint16_t actual_original_sbk_word) noexcept;
[[nodiscard]] std::optional<std::uint8_t>
candidateOriginalCreative1MgmGeneratorEnvelopeRate(
    std::uint16_t generator_id,
    std::uint16_t original_sbk_raw_word) noexcept;
// The real Creative FORMAT-2/SF2 PCOD+0x3BD4 converts merged
// signed timecents to the ORIGINAL 0..128 VOICE DESCRIPTOR attack rate.
// Unlike FORMAT-1/SBK LCOD+0x74F2, this is integer piecewise math.
// Value 128 remains in the descriptor; only the existing VXD note
// register writer transforms its 0x0080 into hardware byte 0x007F.
[[nodiscard]] std::uint8_t originalCreativeSf2VoiceAttackRate(
    std::int16_t merged_sf2_attack_timecents) noexcept;
// Original FORMAT-2 SBAWE.VXD PCOD +0x3BD4: signed merged descriptor
// words -> encoded envelope/LFO words. Includes original integer overflow,
// truncation and bit shifts, never floating-point SoundFont approximations.
// Only explicitly supported operators are accepted; no template defaults
// or actual device-state values are synthesized here.
[[nodiscard]] std::optional<std::uint16_t>
originalCreativeSf2VoiceEnvelopeOrLfoWord(
    std::uint16_t generator_id,std::int16_t merged_word) noexcept;
// The original format-2 melodic note-start scales only these four fields
// by their separately converted key-to-hold/decay fields before encoding.
// Already-merged signed fields are caller inputs; no duplicate bank parser.
[[nodiscard]] std::optional<std::int16_t>
originalCreativeSf2KeyScaledEnvelopeWord(
    std::uint16_t target_generator_id,std::int16_t merged_target,
    std::int16_t merged_key_scaler,std::uint8_t key,std::uint16_t bank) noexcept;
} // namespace awe64::creative
