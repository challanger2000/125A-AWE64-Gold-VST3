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
} // namespace awe64::creative
