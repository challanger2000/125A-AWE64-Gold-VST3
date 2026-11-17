#pragma once
// Original SF2 smpl PCM -> caller-verified EMU8000 physical RAM placement.
// Only straight PCM16 words are transferred; no invented 1996 RAM map.
// All calls are NON-REALTIME (before accepting MIDI/audio callbacks).
// The original Creative ROM is not copied or modified.
#include "OriginalCreativeSf2SourceReader.hpp"
#include <cstdint>
#include <cstddef>
#include <span>
namespace awe64::original_vxd {
struct OriginalSf2RamUploadSource {
    bool original_guest_ram_base_verified=false;
    std::uint32_t original_physical_word_base=0;
};
struct OriginalSf2RamSink {
    void* opaque=nullptr;
    // The target must copy/consume this span synchronously; buffer resides
    // on the caller's stack and may be replaced before the next callback.
    bool (*upload_words)(void*,std::uint32_t,
                         std::span<const std::int16_t>) noexcept=nullptr;
};
enum class OriginalSf2UploadStatus:std::uint8_t {
    Ok,SourceMissing,MalformedBank,AddressOutOfRange,UploadFailed
};
struct OriginalSf2UploadResult {
    OriginalSf2UploadStatus status=OriginalSf2UploadStatus::SourceMissing;
    std::uint32_t words_written=0;
    std::uint32_t callback_count=0;
};
// Unpacks the actual source smpl data (not a synthetic oscillator). Full
// bounds/provenance preflight occurs before the first write; failures in a
// physical RAM upload are NOT rolled back. Caller must invalidate that bank.
[[nodiscard]] OriginalSf2UploadResult upload_verified_sf2_pcm_to_emu8000(
    std::span<const std::uint8_t> raw_sf2,
    OriginalSf2RamUploadSource original,
    OriginalSf2RamSink sink) noexcept;
} // namespace awe64::original_vxd

namespace awe64::original_vxd {
// Autonomously reproduced from SHA-pinned original Creative PCOD+0x26xx / 
// PCOD+0x28AC, and independently checked against the *complete* genuine
// Windows 95 SMLD writer trace of two sequentially loaded RAM banks.
//
// Device capacity is independently authenticated at open/init time; the
// actual bank position is thereafter computed by this original allocation
// routine, NOT guessed from SF2 headers or a user-entered RAM offset.
struct OriginalCreativeRamBankCursor {
    bool original_28mib_device_verified=false;
    bool faulted=false; // partial writes cannot be rolled back
    std::uint32_t next_bank_word=0x200000u;
};
struct OriginalCreativeRamBankPlacement {
    std::uint32_t allocation_start_word=0u;
    std::uint32_t first_sample_word=0u; // allocate_start + 14
    std::uint32_t next_bank_word=0u;    // allocate_start + samples + 16
};
struct OriginalCreativeRamUploadResult {
    OriginalSf2UploadStatus status=OriginalSf2UploadStatus::SourceMissing;
    OriginalCreativeRamBankPlacement placement{};
    std::uint32_t source_pcm_words=0u;
    std::uint32_t physical_sample_writes=0u; // includes guard/duplicate zeros
    std::uint32_t transfer_chunks=0u;
};
// Optional exact source PCOD+0x26AF per-block scratch voice lifecycle.
// Both callbacks are required together. A failed callback permanently
// faults the bank cursor since physical EMU8000 writes cannot be undone.
struct OriginalCreativeRamBlockHooks {
    void* opaque=nullptr;
    bool (*before_block)(void*) noexcept=nullptr;
    bool (*after_block)(void*) noexcept=nullptr;
};
// Source-ported original 1996 four-state 16-bit PCM conversion. The VXD
// returns only the low signed WORD of the filtered signed integer result.
// A special zero fast path is retained: it is NOT equivalent to running a
// generic fixed-coefficient FIR from sample zero without that branch.
// Source-confirmed five x86 control paths of original PCOD+0x28AC.
// Their observed 100/104/110/114-TSC spacing is DIAGNOSTIC, not a
// complete host-independent CPU scheduler.
enum class OriginalCreativePcmX86Path : std::uint8_t {
    OrdinaryNonzeroOlder,
    DirectZero,
    OrdinaryNonzeroOlder2,
    OrdinaryNonzeroOlder3,
    GuardedZero
};
struct OriginalCreativePcmFilter {
    std::int32_t recent=0,older=0,older2=0,older3=0;
    [[nodiscard]] OriginalCreativePcmX86Path original_x86_path(
        std::int16_t sample) const noexcept;
    [[nodiscard]] bool will_take_early_zero_branch(std::int16_t sample) const noexcept;
    // Source-v4 PCM branch observations only; NEVER schedule native
    // audio Port-I/O directly from these researched typical deltas.
    [[nodiscard]] std::uint16_t observed_typical_pcm_loop_delta_tsc(
        std::int16_t next_sample) const noexcept;
    [[nodiscard]] std::uint16_t observed_control_flow_pcm_loop_delta_tsc(
        std::int16_t next_sample) const noexcept;
    [[nodiscard]] std::int16_t next(std::int16_t sample) noexcept;
};
// Physical write order: first 14 zero words, then blocks of <=1024
// *transformed* PCM words with one zero word after EACH block, followed
// by a final group of 17 zero writes. Each block reprograms SMALW at
// base+14+block_offset; the extra zero at a boundary is overwritten by
// the following block. The bank reserves (PCM words + 16) RAM words.
//
// sink.upload_words must synchronously consume the entire span, and maps
// to the original EMU8000 SMALW/SMLD register protocol. This routine is
// non-realtime and does not alter the pinned original DSP.
[[nodiscard]] OriginalCreativeRamUploadResult
upload_original_creative_sf2_ram_bank(
    std::span<const std::uint8_t> original_sf2,
    OriginalCreativeRamBankCursor& ram,
    OriginalSf2RamSink sink,
    OriginalCreativeRamBlockHooks hooks={}) noexcept;
}
