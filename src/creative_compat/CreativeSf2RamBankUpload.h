#pragma once
// P003 research: source-byte-exact SF2 RAM-mono pool transfer to the
// unmodified EMU8000 physical sample memory. NOT a Creative 1996
// allocator, sample remapper or sound-parity certificate.
#include "Chip.h"
#include "CreativeSf2RamSamplePcm.h"
#include "CreativeOriginalRamPortReplay.h"
#include "CreativeCandidateRamAllocator.h"
#include <array>
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
    // Source-captured original port/rewrite counts (zero for raw uploads).
    std::size_t original_port_writes=0;
    std::size_t original_sample_word_writes=0;
    std::size_t original_distinct_overwritten_words=0;
    bool original_source_port_replay_verified=false;
    // Bound to the exact source SF2 bytes, not merely a matching pool size.
    std::array<std::uint8_t,32> source_sf2_sha256{};
    bool source_sf2_sha256_verified=false;
};
// Explicit caller-supplied physical address, never guessed from the
// SoundFont or treated as original Win95 Creative placement. Invoke
// ONLY during initialization, never concurrently with chip.render().
[[nodiscard]] bool p003UploadValidatedMonoSf2PoolToOriginalChip(
    awe64::Chip& chip,std::span<const std::uint8_t> sf2,
    std::uint32_t explicit_physical_word_base,
    P003RamUploadResult& result,std::string& error);
// Candidate autonomous SF2 bank placement + raw PCM transfer to the
// unmodified chip. A transactional allocation handle is returned only
// on SUCCESS; failed format/capacity/transfer releases the reserved
// region immediately. This is NOT yet the original Creative 1996
// memory allocator or signed-sample conversion.
[[nodiscard]] bool p003CandidateAllocateAndUploadMonoSf2(
    awe64::Chip& chip,P003CandidateRamAllocator& allocator,
    std::span<const std::uint8_t> sf2,
    P003CandidateRamAllocation& allocation,
    P003RamUploadResult& result,std::string& error);

// P003 generic source-CAPTURED SF2 RAM bank. For any structurally
// valid mono SF2, the caller supplies a separately authenticated Win95
// Creative SMALW/SMLD upload trace, physical base and final DRAM image.
// The actual Creative trace is the ONLY hardware write path. No assumed
// 0x200010 address, no blind raw smpl memcpy, no one-bank-only SHA gate.
// "source_verified" must be established externally with true original
// Win95 provenance and hashes: this API does NOT perform that audit or
// independently synthesize the original Creative RAM allocator.
// Rejects mismatched/truncated/overlapping source evidence BEFORE writing.
[[nodiscard]] bool p003ReplaySourceCapturedMonoSf2Bank(
    awe64::Chip& chip,
    std::span<const std::uint8_t> sf2,
    std::span<const OriginalRamUploadPortWrite> original_ports,
    std::uint32_t original_physical_base,
    std::span<const std::uint16_t> original_final_words,
    bool original_driver_and_sf2_hashes_independently_verified,
    P003RamUploadResult& result,std::string& error);

// Derive the BANK BASE from actual original Creative SMALW/SMLD
// physical writes, instead of requiring the caller to supply or guess
// 0x200010 or another address. The verified complete original trace
// and final physical DRAM image remain mandatory. No 1996 allocation
// algorithm is conjectured. Activation thread only.
[[nodiscard]] bool p003ReplaySourceCapturedMonoSf2BankAtObservedAddress(
    awe64::Chip& chip,
    std::span<const std::uint8_t> sf2,
    std::span<const OriginalRamUploadPortWrite> original_ports,
    std::span<const std::uint16_t> original_final_words,
    bool original_driver_and_sf2_hashes_independently_verified,
    P003RamUploadResult& result,std::string& error);

// Compare the independent candidate RAM allocator with an authenticated
// Creative Win95 physical bank observation for EXACTLY the same SF2.
// A mismatch reports both positions and releases the candidate lease;
// this comparison does not alter a single EMU8000 RAM word. Each bank
// needs its own independent source capture to prove an allocation rule.
[[nodiscard]] bool p003CompareCandidateAllocationToOriginalCapture(
    P003CandidateRamAllocator& allocator,
    std::span<const std::uint8_t> sf2,
    const P003RamUploadResult& authentic_capture,
    P003CandidateRamAllocation& verified_lease,
    std::string& error);

// Source-captured physical SF2 note address resolver. Unlike raw
// pool resolver this accepts original driver TRANSFORMED RAM bytes,
// provided the FULL final original chip RAM image still matches the
// separately captured source. Never invents a VXD note allocator.
[[nodiscard]] std::optional<OriginalSf2RamPhysicalNotePlan>
p003CandidatePhysicalNoteFromOriginalSourceCapture(
    const awe64::Chip& chip,
    std::span<const std::uint8_t> sf2,
    const P003RamUploadResult& source_capture,
    std::span<const std::uint16_t> original_final_words,
    std::uint16_t bank,std::uint8_t program,
    std::uint8_t key,std::uint8_t velocity) noexcept;

// Reject reusing an otherwise verified original RAM capture with a
// DIFFERENT SoundFont of identical sample-header count and geometry.
[[nodiscard]] bool p003CapturedRamMatchesExactSourceSf2(
    std::span<const std::uint8_t> sf2,
    const P003RamUploadResult& capture) noexcept;

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

// Verify an already replayed original Win95 physical RAM bank WITHOUT
// writing any chip memory. The complete original EMU8000 prelude state
// (including sample RAM and DSP-history) remains untouched.
[[nodiscard]] bool p003VerifyReplayedOriginalWin95OwnerBankWithoutWrites(
    const awe64::Chip& chip,std::span<const std::uint8_t> owner_sf2,
    std::span<const std::uint8_t> authentic_original_ram_pcm16le,
    P003RamUploadResult& result,std::string& error);

// Resolve a MIDI note against the ACTUAL just-uploaded raw PCM16 pool.
// This binds the caller's SF2 to physical memory by comparing ALL words
// in every selected source sample before returning chip voice addresses.
// It does not authorize original Creative 1996 address allocation or
// the unverified native 61-register NoteOn sequence.
[[nodiscard]] std::optional<OriginalSf2RamPhysicalNotePlan>
p003CandidatePhysicalNoteFromVerifiedRawPool(
    const awe64::Chip& chip,std::span<const std::uint8_t> sf2,
    const P003RamUploadResult& upload,std::uint16_t bank,
    std::uint8_t program,std::uint8_t key,std::uint8_t velocity) noexcept;

} // namespace awe64::creative
