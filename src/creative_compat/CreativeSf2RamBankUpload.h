#pragma once
// P003 research: source-byte-exact SF2 RAM-mono pool transfer to the
// unmodified EMU8000 physical sample memory. NOT a Creative 1996
// allocator, sample remapper or sound-parity certificate.
#include "Chip.h"
#include "CreativeSf2RamSamplePcm.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
namespace awe64::creative {
struct P003RamUploadResult {
    std::uint32_t physical_word_base=0;
    std::size_t source_pool_words=0;
    std::size_t sample_headers=0;
    bool entire_pool_readback_equal=false;
    bool original_creative_source_transformation_verified=false;
};
// Explicit caller-supplied physical address, never guessed from the
// SoundFont or treated as original Win95 Creative placement. Invoke
// ONLY during initialization, never concurrently with chip.render().
[[nodiscard]] bool p003UploadValidatedMonoSf2PoolToOriginalChip(
    awe64::Chip& chip,std::span<const std::uint8_t> sf2,
    std::uint32_t explicit_physical_word_base,
    P003RamUploadResult& result,std::string& error);
// Strict original-Win95-owner profile: SHA-attest BOTH the authentic
// 555,754-byte source SF2 and the authentic 521,842-byte physical RAM
// word snapshot; run the 3-tap original-driver-observed conversion and
// COMPARE ALL 260,921 WORDS BEFORE any chip memory is modified.
// Unlike the generic raw PCM transfer above, this is source-equivalent
// for that one particular Creative bank only. Windows-only BCrypt gate.
[[nodiscard]] bool p003UploadExactOriginalWin95ConvertedOwnerBank(
    awe64::Chip& chip,std::span<const std::uint8_t> owner_sf2,
    std::span<const std::uint8_t> authentic_original_ram_pcm16le,
    P003RamUploadResult& result,std::string& error);

} // namespace awe64::creative
