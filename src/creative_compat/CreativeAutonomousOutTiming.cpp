#include "CreativeAutonomousOutTiming.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace awe64::creative {
namespace {
// P003: two independently recorded Win95 sessions (18:43 and 18:53)
// had identical relative physical OUT CPU-TSC deltas for each of 324
// C4/D4 note-register writes, despite DIFFERENT 44.1 kHz sample frames.
// This confirms that driver work and intrasample CPU phase must be
// modeled separately. All coefficients remain empirical, source
// protocol-category estimates, NOT a cycle-exact 1996 VXD emulator.
// No MIDI key, program, source event timestamp, or note ID is read.
constexpr std::uint32_t kFirstVoiceSearchBridge=2340u;
constexpr std::uint32_t kSecondVoiceSearchBridge=2314u;
constexpr std::uint32_t kVoiceProbeOut=109u;
constexpr std::uint32_t kVoiceSelectOut=78u;
constexpr std::uint32_t kIndexAfterData620=70u;
constexpr std::uint32_t kIndexAfterDataA20=72u;
constexpr std::uint32_t kIndexAfterDataA22=76u;
constexpr std::uint32_t kIndexAfterDataE20=82u;
constexpr std::uint32_t kIndexAfterIndex=97u;
// Longer driver work follows particular EMU8000 register SELECTOR
// classes after an A20 data transfer: E22 0x80 (control transfer) and
// E22 0x60 (another voice's initialization). Match the *register class*
// in the independent native OUT stream, not the A20 payload value:
// SoundFont/velocity changes may alter that data word. The observed
// inter-voice CPU variability (~75 cycles) is unresolved, hence a mean
// cost rather than a hardcoded voice/note-specific clock trace.
constexpr std::uint32_t kAfterA20ControlTransfer=236u;
constexpr std::uint32_t kAfterA20FinalEnvelope=162u;
constexpr std::uint32_t kReleaseNextRegister=76u;
constexpr std::uint32_t kReleaseSecondVoiceWork=177u;
constexpr std::size_t kObservedMinPreludeWrites=12u;
constexpr std::size_t kMaxPreludeWrites=24u;
constexpr std::uint64_t kMaxEstimatedCycles=
    (kP003GuestCpuHzNumerator*30u)/kP003GuestCpuHzDenominator;

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
            return i<firstPreludeEnd?128u:119u;
        return 0u;
    }
    // E22 index selects and subsequent 16/32-bit data transfers share
    // the same guest CPU tick in the measured physical callback trace.
    // Cost the NEXT index select, not every hardware OUT individually.
    if(current.port!=0xe22u)return 0u;
    switch(previous.port) {
        case 0x622u:return kIndexAfterData620;
        case 0xa20u:
            // These E22 high address bits identify which EMU8000
            // register class the original driver next initializes.
            // No MIDI key/preset, data payload, source note clock or
            // voice-number exception is used to infer the CPU work.
            if((current.value & 0xe0u)==0x80u)
                return kAfterA20ControlTransfer;
            if((current.value & 0xe0u)==0x60u)
                return kAfterA20FinalEnvelope;
            return kIndexAfterDataA20;
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
       static_cast<std::uint64_t>(phase_cycles)*kP003ChipHz*
           kP003GuestCpuHzDenominator>=kP003GuestCpuHzNumerator)
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
        // 86Box pit_set_clock() repairs 133333333 to 400000000/3.
        // Correct rational frame map, source-derived, NO empirical phase.
        const std::uint64_t absoluteNumerator=
            (static_cast<std::uint64_t>(phase_cycles)+cycles)*kP003ChipHz*
            kP003GuestCpuHzDenominator;
        const auto frame=static_cast<std::uint32_t>(
            absoluteNumerator/kP003GuestCpuHzNumerator);
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
            kP003GuestCpuHzNumerator/
            (2u*kP003GuestCpuHzDenominator*kP003ChipHz));
    return p003CandidateAutonomousNoteTimelineAtPhase(
        native,note_off,kUnverifiedMidSampleCpuPhase,out,count,stats);
}
} // namespace awe64::creative
