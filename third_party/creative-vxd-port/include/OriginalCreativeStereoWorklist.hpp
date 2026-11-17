#pragma once
// Portable port of original SBAWE.VXD LCOD+0x544C..0x5560
// 148-byte SoundFont descriptor linking and stereo voice assignment.
// No original driver bytes, dynamic allocation, Win95 or 86Box dependency.
#include "OriginalCreativeVxdLive.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace awe64::original_vxd {
constexpr std::size_t original_descriptor_size = 0x94;
constexpr std::size_t max_note_descriptors = 64;
using Descriptor = std::array<std::uint8_t,original_descriptor_size>;

enum class WorkStatus : std::uint8_t { Ok, BadInput, TooManyDescriptors, MissingPair };
// process_type means ORIGINAL temporary descriptor worklist type, not SF2
// sampleType and not a new MIDI channel. The original assigns 4 to the
// paired partner, 2 to the triggering right, and 1 to unpaired samples.
struct WorkItem {
    std::uint8_t source_index = no_voice;
    std::uint8_t process_type = 0; // 4: paired-left; 2: right reuse; 1: single
    std::uint8_t paired_source = no_voice;
};
struct Worklist {
    WorkStatus status = WorkStatus::BadInput;
    std::array<WorkItem,max_note_descriptors> items{};
    std::size_t count = 0;
    std::size_t stereo_pairs = 0;
};

// Original VXD searches partner by 16-bit sampleID == current sampleLink.
// It does NOT independently check reciprocal links in this assembly block;
// the SoundFont loader is responsible for validating its source resources.
// A missing right->partner link follows the original mono fallback. Strict
// SoundFont validation is a separate API responsibility.
[[nodiscard]] Worklist original_stereo_worklist(
    std::span<const Descriptor> descriptors) noexcept;

struct PhysicalWorkItem {
    WorkItem source{};
    std::uint8_t physical_voice = no_voice;
};
// Map one matched pair onto one already-selected original mode1 pair.
// Partner (first work item, type 4) uses base+1; triggering right
// (second item, type 2) uses base. Original +0x5549/+0x554D.
[[nodiscard]] bool original_assign_stereo_pair(
    WorkItem partner, WorkItem right,
    Allocation const& selected,
    PhysicalWorkItem& first, PhysicalWorkItem& second) noexcept;
} // namespace awe64::original_vxd
