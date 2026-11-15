#include "CreativeAutonomousOutTiming.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace awe64::creative {
namespace {
// Owner's original CPU/EMU8000 V3 evidence showed 133.333333MHz and
// two expensive voice-search/driver bookkeeping gaps of ~2.3k cycles,
// followed by short index+data groups. These costs are structural and
// independent of MIDI key/program/velocity and source port times.
// They are still EMPIRICAL estimates (not disassembled VXD cycles).
constexpr std::uint32_t kFirstVoiceSearchBridge=2340u;
constexpr std::uint32_t kSecondVoiceSearchBridge=2314u;
constexpr std::uint32_t kVoiceProbeOut=110u;
constexpr std::uint32_t kVoiceSelectOut=80u;
constexpr std::uint32_t kProbeToResetOut=123u;
constexpr std::uint32_t kIndexAfterData620=72u;
constexpr std::uint32_t kIndexAfterDataA20=90u;
constexpr std::uint32_t kIndexAfterDataA22=82u;
constexpr std::uint32_t kIndexAfterDataE20=87u;
constexpr std::uint32_t kIndexAfterIndex=110u;
constexpr std::uint32_t kReleaseNextRegister=76u;
constexpr std::uint32_t kReleaseSecondVoiceWork=180u;
constexpr std::size_t kObservedMinPreludeWrites=12u;
constexpr std::size_t kMaxPreludeWrites=24u;
constexpr std::uint64_t kMaxEstimatedCycles=
    (kP003GuestCpuHz*30u);

bool allowed(const awe64::ChipPortWrite& p) noexcept {
    return p.port==0xe22u || p.port==0xe20u ||
           p.port==0xa20u || p.port==0xa22u ||
           p.port==0x620u || p.port==0x622u;
}
std::uint32_t cost(const awe64::ChipPortWrite& previous,
                   const awe64::ChipPortWrite& current,
                   const std::size_t i,
                   const bool release,
                   const std::size_t firstPreludeEnd,
                   const std::size_t secondPreludeEnd) noexcept {
    if(release) {
        // Driver release is usually index+data word pairs.
        // The second voice's release/ownership scan takes longer
        // than the other register pair transitions.
        if(current.port==0xe22u && previous.port!=0xe22u)
            return i==4u?kReleaseSecondVoiceWork:kReleaseNextRegister;
        return 0u;
    }
    if(i==firstPreludeEnd)return kFirstVoiceSearchBridge;
    if(i==secondPreludeEnd)return kSecondVoiceSearchBridge;
    if(i<secondPreludeEnd) {
        // Voice search can perform a variable number of consecutive
        // E22 index probes; count ACTUAL native operations, never
        // look up a pitch, patch, channel or original note timestamp.
        if(previous.port==0xe22u && current.port==0xe22u)
            return (current.value&0x80u)?
                kVoiceSelectOut:kVoiceProbeOut;
        if(previous.port==0xa20u && current.port==0xe22u)
            return kProbeToResetOut;
        return 0u;
    }
    // E22 index selects and subsequent 16/32-bit data transfers share
    // the same guest CPU tick in the measured physical callback trace.
    // Cost the NEXT index select, not every hardware OUT individually.
    if(current.port!=0xe22u)return 0u;
    switch(previous.port) {
        case 0x622u:return kIndexAfterData620;
        case 0xa20u:return kIndexAfterDataA20;
        case 0xa22u:return kIndexAfterDataA22;
        case 0xe20u:return kIndexAfterDataE20;
        case 0xe22u:return kIndexAfterIndex;
        default:return 0u;
    }
}
} // namespace

bool p003CandidateAutonomousNoteTimelineAtPhase(
    const std::span<const awe64::ChipPortWrite> native,
    const bool note_off,
    const std::uint32_t phase_cycles,
    const std::span<SourceClockedPort> out,
    std::size_t& count,
    P003AutonomousTimingStats* stats) noexcept {
    count=0u;
    if(stats)*stats={};
    if(native.empty() ||
       native.size()>SourceClockedPortPlayer::kMaxEvents ||
       native.size()>out.size() ||
       static_cast<std::uint64_t>(phase_cycles)*kP003ChipHz>=kP003GuestCpuHz)
        return false;
    // Validate the WHOLE transaction before producing a schedule.
    for(const auto& write:native)
        if(!allowed(write))return false;

    std::size_t firstPreludeEnd=0u,secondPreludeEnd=0u;
    if(note_off) {
        if(native.size()>16u)return false;
    }else {
        // First two actual completed 620/622 reset-write groups
        // delimit the original two-voice allocation phase.
        for(std::size_t i=0;i<native.size() &&
                i<kMaxPreludeWrites && !secondPreludeEnd;++i) {
            if(native[i].port==0x622u && native[i].value==0u) {
                if(!firstPreludeEnd)firstPreludeEnd=i+1u;
                else secondPreludeEnd=i+1u;
            }
        }
        if(firstPreludeEnd<6u ||
           secondPreludeEnd<kObservedMinPreludeWrites ||
           secondPreludeEnd>kMaxPreludeWrites ||
           native.size()<=secondPreludeEnd ||
           native.size()-secondPreludeEnd<16u)
            return false;
    }

    std::uint64_t cycles=0u;
    std::uint32_t previousFrame=0u;
    // At the beginning of a sample, the CPU has a fractional phase.
    // Keeping it in integer CPU-cycle units permits note-off crossing
    // the sample boundary (including 6+2 or 4+4 write distributions)
    // without an original NoteOff frame lookup table.
    for(std::size_t i=0;i<native.size();++i) {
        if(i)cycles+=cost(native[i-1u],native[i],i,note_off,
                           firstPreludeEnd,secondPreludeEnd);
        if(cycles>kMaxEstimatedCycles)return false;
        const std::uint64_t absoluteNumerator=
            (static_cast<std::uint64_t>(phase_cycles)+cycles)*kP003ChipHz;
        const auto frame=static_cast<std::uint32_t>(
            absoluteNumerator/kP003GuestCpuHz);
        if(frame<previousFrame || frame>kP003ChipHz*30u)
            return false;
        out[i]={frame,native[i].port,native[i].value};
        previousFrame=frame;
    }
    count=native.size();
    if(stats) {
        stats->writes=count;
        stats->initial_voice_prelude_writes=secondPreludeEnd;
        stats->final_relative_frame=previousFrame;
        stats->estimated_guest_cpu_cycles=cycles;
        stats->assumed_intrasample_phase_cycles=phase_cycles;
    }
    return true;
}

bool p003CandidateAutonomousNoteTimeline(
    const std::span<const awe64::ChipPortWrite> native,
    const bool note_off,
    const std::span<SourceClockedPort> out,
    std::size_t& count,
    P003AutonomousTimingStats* stats) noexcept {
    // Sample-accurate VST3 host MIDI has no original Win95 fractional
    // CPU scheduler phase. Use a deterministic mid-sample placeholder;
    // NEVER report this as authenticated Win95/Creative timing.
    constexpr std::uint32_t kUnverifiedMidSampleCpuPhase=
        static_cast<std::uint32_t>(
            kP003GuestCpuHz/(2u*kP003ChipHz));
    return p003CandidateAutonomousNoteTimelineAtPhase(
        native,note_off,kUnverifiedMidSampleCpuPhase,out,count,stats);
}
} // namespace awe64::creative
