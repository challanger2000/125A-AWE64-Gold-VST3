#pragma once
// P007AR - bounded host-side event placement INSIDE the VST3 process block.
// This does not assert sample-accurate timestamp delivery to an asynchronous
// Windows 95 guest: WinMM/COM2 emulation has its own clock and FIFO latency.
// Host events may not reach the guest before their requested block offset.
// Input event lists are expected in nondecreasing offset order by VST3 hosts;
// malformed/unsorted offsets are clamped to the current rendered cursor.
#include <algorithm>
#include <cstdint>
namespace p007::midi_offset {
inline std::uint32_t place(std::int32_t eventOffset,
                           std::int32_t blockFrames,
                           std::uint32_t rendered) noexcept {
    const auto size=blockFrames>0
        ? static_cast<std::uint32_t>(blockFrames):0u;
    const auto at=eventOffset<=0 ? 0u :
        std::min(static_cast<std::uint32_t>(eventOffset),size);
    return std::max(std::min(rendered,size),at);
}
} // namespace p007::midi_offset
