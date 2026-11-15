#include "CreativeAutonomousOutTiming.h"
#include <array>
#include <cstddef>
#include <cstdint>
namespace awe64::creative {
namespace {
// P003 original owner's SHA-pinned VM/86box.cfg actually runs the
// Pentium P54C at 133333333 Hz (not 200MHz). The authentic guest CPU
// TSC to 44100Hz EMU8000 frame differential independently measured
// 133333330.6 Hz on the P003-20261115-181229 capture.
// Earlier experimental coefficients were incorrectly labeled as
// 200MHz guest CPU cycles. They were fitted to the SAME source sample
// clock and have been rescaled by 2/3 solely to preserve the existing
// provisional sample-frame candidate. These are still NOT actual
// 1996 Creative VXD instruction timings. No validated autonomous
// multi-run PCM equivalence is claimed; private research flag ONLY.
constexpr std::uint64_t kGuestCpuHz=133333333u;
constexpr std::uint64_t kChipHz=44100u;
constexpr std::uint64_t kIndexOutCycles=13u;
constexpr std::uint64_t kDataOutCycles=45u;
constexpr std::uint64_t kInitialBusPhaseCycles=1347u;
constexpr std::uint64_t kExtraInitialWriteCycles=194u;
constexpr std::size_t kObservedMinPreludeWrites=12u;
constexpr std::size_t kMaxPreludeWrites=24u;

bool allowed(const awe64::ChipPortWrite& p) noexcept {
    return p.port==0xe22u || p.port==0xe20u ||
           p.port==0xa20u || p.port==0xa22u ||
           p.port==0x620u || p.port==0x622u;
}
std::uint32_t cyclesToFrames(const std::uint64_t cycles) noexcept {
    return static_cast<std::uint32_t>(
        cycles*kChipHz/kGuestCpuHz);
}
} // namespace
bool p003CandidateAutonomousNoteTimeline(
    const std::span<const awe64::ChipPortWrite> native,
    const bool note_off,
    const std::span<SourceClockedPort> out,
    std::size_t& count,
    P003AutonomousTimingStats* stats) noexcept {
    count=0u;
    if(stats)*stats={};
    if(native.empty() ||
       native.size()>SourceClockedPortPlayer::kMaxEvents ||
       native.size()>out.size())
        return false;
    for(const auto& write:native)
        if(!allowed(write))return false;
    if(note_off) {
        // Short release sequences finish before the next sample tick;
        // reject larger, unmeasured release transactions.
        if(native.size()>16u)return false;
        for(std::size_t i=0;i<native.size();++i)
            out[i]={0u,native[i].port,native[i].value};
        count=native.size();
        if(stats)stats->writes=count;
        return true;
    }
    // The supported source compiler currently prepares two voices by
    // writing 620/622 reset pairs. Treat the FIRST TWO completed pairs
    // as voice-allocation prelude. No MIDI note number or source clock
    // offsets are consulted here.
    std::array<std::size_t,2> ends{};
    std::size_t groups=0u;
    for(std::size_t i=0u;
        i<native.size() && i<kMaxPreludeWrites && groups<2u;++i) {
        if(native[i].port==0x622u && native[i].value==0u)
            ends[groups++]=i+1u; // exclusive index
    }
    if(groups!=2u || ends[0]<6u ||
       ends[1]<kObservedMinPreludeWrites ||
       ends[1]>kMaxPreludeWrites ||
       native.size()<=ends[1] ||
       native.size()-ends[1]<16u)
        return false;
    // Two separate guest driver voice-allocation iterations happen
    // before its bulk register programming. Model each iteration as a
    // sample tick; the remaining OUTs consume virtual guest CPU cycles.
    std::uint64_t cycles=kInitialBusPhaseCycles+
        (ends[1]-kObservedMinPreludeWrites)*kExtraInitialWriteCycles;
    std::uint32_t previous=0u;
    for(std::size_t i=0u;i<native.size();++i) {
        std::uint32_t frame;
        if(i<ends[0])frame=0u;
        else if(i<ends[1])frame=1u;
        else {
            frame=2u+cyclesToFrames(cycles);
            cycles+=native[i].port==0xe22u?
                kIndexOutCycles:kDataOutCycles;
        }
        if(frame<previous || frame>44100u*30u)return false;
        out[i]={frame,native[i].port,native[i].value};
        previous=frame;
    }
    count=native.size();
    if(stats) {
        stats->writes=count;
        stats->initial_voice_prelude_writes=ends[1];
        stats->final_relative_frame=previous;
    }
    return true;
}
} // namespace awe64::creative
