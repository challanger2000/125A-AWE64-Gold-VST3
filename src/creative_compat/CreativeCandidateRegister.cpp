#include "CreativeCandidateRegister.h"
#include <algorithm>
namespace awe64::creative {
namespace {
// 127-level SHARED, bank-neutral measurement-derived velocity coordinate.
// Derived only from owner's two *independently captured* original Creative
// Win95 127-velocity register datasets by integer inversion of their common
// attenuation curve. It is NOT a per-preset lookup table; GM IDs are absent.
// 0 is reserved/unused because velocity 0 is NoteOff. Numeric source files
// remain private (see P002-ORIGINAL-CREATIVE-ALL-127-VELOCITY-EVIDENCE.md).
// The arithmetic structure (combined term, 8/3) is separately corroborated
// by historical Linux OSS awe_wave.c (v2.6.12). Its GPL code is NOT copied.
constexpr std::array<std::uint8_t,128> kSharedVelocityLevel{{
    0,49,48,47,46,45,44,43,42,42,41,40,39,38,37,36,
    36,35,34,33,33,32,31,30,30,29,28,28,27,26,26,25,
    25,24,24,23,22,22,21,21,20,20,19,19,19,18,18,17,
    17,16,16,16,15,15,15,14,14,14,13,13,13,12,12,12,
    11,11,11,11,10,10,10,10,9,9,9,9,9,8,8,8,
    8,8,7,7,7,7,7,6,6,6,6,6,6,5,5,5,
    5,5,5,5,4,4,4,4,4,4,3,3,3,3,3,3,
    2,2,2,2,2,2,1,1,1,1,1,0,0,0,0,0
}};
static_assert(kSharedVelocityLevel.size()==128);
static_assert(kSharedVelocityLevel[1]==49 && kSharedVelocityLevel[127]==0);
}
std::uint8_t originalCreativeMeasuredVelocityLevel(
    const std::uint8_t velocity) noexcept {
    return kSharedVelocityLevel[velocity];
}
std::optional<InitialIfatn> candidateInitialIfatn(
    const std::uint8_t velocity, const CandidateVoiceParameters& bank) noexcept {
    if (velocity==0 || velocity>127) return std::nullopt;
    const unsigned common = static_cast<unsigned>(bank.combined_volume_index)
                          + kSharedVelocityLevel[velocity];
    const unsigned attenuation = std::min(255u,(common*8u)/3u
                                + static_cast<unsigned>(bank.sample_attenuation));
    const unsigned base = static_cast<unsigned>(bank.initial_cutoff);
    const unsigned cutoff = bank.velocity_cutoff_enabled
         ? std::min(255u,((std::max(70u,static_cast<unsigned>(velocity))*base)+0xA0u)>>7)
         : base;
    return InitialIfatn{static_cast<std::uint8_t>(cutoff),
                        static_cast<std::uint8_t>(attenuation),
                        static_cast<std::uint16_t>((cutoff<<8)|attenuation)};
}
std::optional<InitialIfatn> candidateOriginalIfatnFromEncodedAttack(
    std::uint8_t velocity, const CandidateVoiceParameters& original_terms,
    std::uint16_t bank, std::uint16_t encoded_vol_attack_rate) noexcept {
    // This predicate is recovered independently from the exact installed
    // original SBAWE.VXD branch (LE locked-code near 0x4D36). The original
    // generator34->encoded-rate conversion remains outside this function.
    // Original Format-2/SF2 voice builder legitimately encodes the
    // descriptor attack as 128. Only the later ATKHLDV port writer maps
    // exact 0x0080 to 0x007F; the velocity-filter gate tests <125.
    // Keeping descriptor 128 here is essential for original GM100.
    if(encoded_vol_attack_rate>128)return std::nullopt;
    auto terms=original_terms;
    terms.velocity_cutoff_enabled=(bank<128 && encoded_vol_attack_rate<125);
    return candidateInitialIfatn(velocity,terms);
}
std::optional<InitialIfatn> candidateOriginalSf2IfatnFromMergedCutoff(
    const std::uint8_t velocity,
    const CandidateVoiceParameters& verified_terms,
    const std::int16_t merged_sf2_cutoff,
    const std::uint16_t creative_bank,
    const std::uint16_t encoded_vol_attack_rate) noexcept {
    auto terms=verified_terms;
    terms.initial_cutoff=static_cast<std::uint8_t>(
        std::clamp<std::int32_t>(merged_sf2_cutoff,0,255));
    // Uses the ORIGINAL melodic-bank and <125 encoded-attack filter gate;
    // descriptor attack128 is legal but never velocity-scales the cutoff.
    return candidateOriginalIfatnFromEncodedAttack(
        velocity,terms,creative_bank,encoded_vol_attack_rate);
}
std::optional<std::array<PortWord,2>> candidateInitialIfatnWrites(
    const MidiMessage& midi, const std::uint8_t physicalVoice,
    const CandidateVoiceParameters& bank) noexcept {
    if (physicalVoice>=32 || midi.kind!=MidiKind::NoteOn || midi.data2==0)
        return std::nullopt;
    const auto reg=candidateInitialIfatn(midi.data2,bank);
    if (!reg) return std::nullopt;
    return std::array<PortWord,2>{{
        {0xE22u,static_cast<std::uint16_t>(0x20u|physicalVoice)}, // DATA3 index 1
        {0xE20u,reg->word}
    }};
}
}
