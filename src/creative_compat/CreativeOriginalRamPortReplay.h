#pragma once
// P002 source-bound (not independently synthesised) original Win95
// Creative SMALW/SMLD RAM upload trace replay on the UNMODIFIED
// calibrated EMU8000. This is a strictly ordered hardware oracle for a
// caller-supplied ORIGINAL guest capture, NOT a replacement 1996
// Creative memory allocator, bank compiler or sample uploader.
//
// The caller MUST independently verify the Win95/SBAWE.VXD trace,
// its sample-bank input, original 86Box fork and absolute clock. The
// provenance bool is a necessary, never sufficient, gate. No private
// Creative data are compiled in or committed to Git.
#include "Chip.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace awe64::creative {
struct OriginalRamUploadPortWrite {
    std::uint64_t original_chip_frame=0u;
    std::uint16_t port=0u;
    std::uint16_t value=0u;
};
struct OriginalRamUploadReplayResult {
    std::size_t exact_port_writes=0u;
    std::size_t original_sample_words=0u;
    std::uint32_t first_word_address=0u;
    std::uint32_t last_word_address=0u;
    std::uint64_t first_chip_frame=0u;
    std::uint64_t last_chip_frame=0u;
};

// Preflights all entries and memory bounds BEFORE ANY chip write,
// then replays only actual captured E22/A20/A22 original SMALW/SMLD
// operations to the unchanged chip port bridge. No allocations. No
// chip replacement or post-hoc sample address correction. A failed
// preflight does not mutate the chip or original upload stream.
// Call only off the audio/render thread with a pinned original guest
// RAM-upload trace that has been independently authenticated.
[[nodiscard]] std::optional<OriginalRamUploadReplayResult>
candidateReplayOriginalCreativeCapturedRamUpload(
    awe64::Chip& pinned_original_emu8000,
    std::span<const OriginalRamUploadPortWrite> source_original_trace,
    bool independently_verified_original_Win95_Creative_RAM_trace) noexcept;
} // namespace awe64::creative
