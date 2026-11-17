#include "OriginalCreativeVxdLive.hpp"
#include <algorithm>
#include <cstdint>

namespace awe64::original_vxd {
namespace {
constexpr std::uint32_t invalid_pair_score = 0x000fffffu;
constexpr std::uint32_t extra_pair_penalty = 0x0c00u;

[[nodiscard]] bool valid(DeviceState const& s, PortIo const& io) noexcept {
    return s.physical_capacity>=1 && s.physical_capacity<=physical_voices &&
           io.out8 && io.out16 && io.in16 && io.in32;
}

struct Live {
    PortIo io;
    Allocation& result;
    void observe(StereoHelperKind kind,std::uint8_t voice) const noexcept {
        if(io.stereo_helper_observer)
            io.stereo_helper_observer(io.stereo_helper_opaque,{kind,voice});
    }
    [[nodiscard]] bool index(std::uint16_t value) noexcept {
        if(!io.out8(io.opaque,0x0e22,static_cast<std::uint8_t>(value))) return false;
        ++result.io_writes;
        return true;
    }
    [[nodiscard]] bool vtft(std::uint8_t voice, std::uint32_t& out) noexcept {
        if(!index(static_cast<std::uint16_t>(0x60u|voice)) ||
           !io.in32(io.opaque,0x0620,out)) return false;
        ++result.io_reads;
        ++result.vtft_reads;
        observe(StereoHelperKind::Vtft,voice);
        return true;
    }
    [[nodiscard]] bool dcysusv(std::uint8_t voice, std::uint16_t& out) noexcept {
        if(!index(static_cast<std::uint16_t>(0xa0u|voice)) ||
           !io.in16(io.opaque,0x0a20,out)) return false;
        ++result.io_reads;
        ++result.dcysusv_reads;
        observe(StereoHelperKind::Dcysusv,voice);
        return true;
    }
    [[nodiscard]] bool ccca(std::uint8_t voice, std::uint32_t& out) noexcept {
        if(!index(static_cast<std::uint16_t>(0x00u|voice)) ||
           !io.in32(io.opaque,0x0a20,out)) return false;
        ++result.io_reads;
        ++result.ccca_reads;
        observe(StereoHelperKind::Ccca,voice);
        return true;
    }
    [[nodiscard]] bool release(std::uint8_t voice) noexcept {
        if(!index(static_cast<std::uint16_t>(0xa0u|voice)) ||
           !io.out16(io.opaque,0x0a20,0x00ffu)) return false;
        ++result.io_writes;
        ++result.completed_release_writes;
        observe(StereoHelperKind::Release,voice);
        return true;
    }
};

[[nodiscard]] bool forbidden(VoiceRecord const& v, std::uint32_t count) noexcept {
    return count<=2u && v.blocked;
}

// Exact integer priority from +0x4074..+0x40B3 / +0x41FA..+0x423B.
// Returns false only on a hardware read failure.
[[nodiscard]] bool score(VoiceRecord const& row, std::uint8_t voice,
                         Live& live, std::uint32_t& value) noexcept {
    std::uint32_t vtft=0;
    if(!live.vtft(voice,vtft)) return false;
    value=vtft>>16u;
    if(row.key_marker!=0x0ffeu) {
        value+=(row.key_marker&0x1000u)?0x1200u:0x0200u;
        if((row.key_marker&0xffu)==0xffu) value+=0x0300u;
        else {
            std::uint16_t sus=0;
            if(!live.dcysusv(voice,sus)) return false;
            if((sus&0x8000u)==0u) value+=0x1300u;
        }
    }
    return true;
}

[[nodiscard]] bool sample_finished(VoiceRecord const& row, std::uint8_t voice,
                                   Live& live, bool& finished) noexcept {
    finished=false;
    if(row.sample_end==0u) return true; // NO speculative hardware read
    std::uint32_t cursor=0;
    if(!live.ccca(voice,cursor))return false;
    finished=row.sample_end <= (cursor&0x00ffffffu);
    return true;
}
} // namespace

namespace {
// Shared original LCOD+0x60E4 release body: ordinary NoteOff and CC64
// pedal-up both call this routine. There is no float ADSR approximation.
[[nodiscard]] bool release_one(VoiceRecord& row,std::uint8_t voice,
                               PortIo io,ReleaseResult& result) noexcept {
    auto index=[&](const std::uint8_t reg) noexcept -> bool {
        if(!io.out8(io.opaque,0x0e22u,static_cast<std::uint8_t>(reg|voice)))
            return false;
        ++result.io_writes;
        return true;
    };
    auto write16=[&](const std::uint8_t reg,std::uint16_t value) noexcept -> bool {
        if(!index(reg) || !io.out16(io.opaque,0x0a20u,value))return false;
        ++result.io_writes;
        return true;
    };
    // A partial hardware failure can leave the physical voice in a
    // half-released state; never misreport success or restore a fake marker.
    row.key_marker=0xff00u;
    if(!write16(0xa0u,static_cast<std::uint16_t>(0x8000u|row.volume_release)) ||
       !write16(0xe0u,static_cast<std::uint16_t>(0x8000u|row.modulation_release))) {
        result.status=Status::PortError;
        return false;
    }
    if(row.release_sample_end!=0u) {
        const std::uint32_t end=row.release_sample_end;
        if(end>0xfffffbu){result.status=Status::MissingSource;return false;}
        std::uint32_t current=0;
        if(!index(0xe0u) || !io.in32(io.opaque,0x0620u,current)) {
            result.status=Status::PortError;return false;
        }
        ++result.io_reads;
        if(!index(0xe0u) ||
           !io.out32(io.opaque,0x0620u,(current&0xff000000u)|(end+4u))) {
            result.status=Status::PortError;return false;
        }
        ++result.io_writes;
        if(!index(0xc0u) || !io.in32(io.opaque,0x0620u,current)) {
            result.status=Status::PortError;return false;
        }
        ++result.io_reads;
        if(!index(0xc0u) ||
           !io.out32(io.opaque,0x0620u,(current&0xff000000u)|end)) {
            result.status=Status::PortError;return false;
        }
        ++result.io_writes;
        row.sample_end=end;
    }
    row.key_marker=0x0ffeu;
    ++result.released;
    return true;
}
[[nodiscard]] bool release_io_valid(DeviceState const& state,PortIo io) noexcept {
    return state.physical_capacity>=1u &&
           state.physical_capacity<=physical_voices &&
           io.out8 && io.out16 && io.in32 && io.out32;
}
} // namespace

ReleaseResult release_note(DeviceState& state,std::uint8_t channel,
                           std::uint8_t key,PortIo io) noexcept {
    ReleaseResult result;
    if(channel>=16u || key>127u || state.physical_capacity==0u ||
       state.physical_capacity>physical_voices)return result;
    // Hardware callbacks are required ONLY for actual voice release.
    // The original sustain-ON branch writes state, not registers.
    result.status=Status::Found;
    const auto desired=static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(channel)<<8u)|key);
    for(std::uint32_t n=0;n<state.physical_capacity;++n) {
        auto& row=state.voices[n];
        if(row.key_marker!=desired)continue;
        ++result.matched;
        if(state.channel_sustain[channel]) {
            row.key_marker=static_cast<std::uint16_t>((desired&0xff00u)|0xffu);
            ++result.deferred;
            continue;
        }
        if(!release_io_valid(state,io) || row.release_sample_end>0xfffffbu) {
            result.status=Status::MissingSource;return result;
        }
        if(!release_one(row,static_cast<std::uint8_t>(n),io,result))return result;
    }
    return result;
}

ReleaseResult controller_sustain(DeviceState& state,std::uint8_t channel,
                                 std::uint8_t value,PortIo io) noexcept {
    ReleaseResult result;
    if(channel>=16u || value>127u || state.physical_capacity==0u ||
       state.physical_capacity>physical_voices)return result;
    result.status=Status::Found;
    if(value>=64u) {
        state.channel_sustain[channel]=true;
        return result;
    }
    state.channel_sustain[channel]=false;
    const auto deferred=static_cast<std::uint16_t>(
        (static_cast<std::uint32_t>(channel)<<8u)|0xffu);
    for(std::uint32_t n=0;n<state.physical_capacity;++n) {
        auto& row=state.voices[n];
        if(row.key_marker!=deferred)continue;
        ++result.matched;
        if(!release_io_valid(state,io) || row.release_sample_end>0xfffffbu) {
            result.status=Status::MissingSource;return result;
        }
        if(!release_one(row,static_cast<std::uint8_t>(n),io,result))return result;
    }
    return result;
}

ReleaseResult original_channel_all_notes_off(DeviceState& state,std::uint8_t channel,
                                   bool observe_sustain,PortIo io) noexcept {
    ReleaseResult result;
    if(channel>=16u || state.physical_capacity==0u ||
       state.physical_capacity>physical_voices)return result;
    // The original routine at LCOD+0x6C82 visits physical voice rows in
    // ascending order, ignoring idle marker 0x0FFE and checking the
    // channel in the high key-marker byte. CC123 consults sustain, CC120
    // deliberately does not. An already-deferred channel:FF is included.
    const bool defer=observe_sustain && state.channel_sustain[channel];
    std::uint32_t matches=0u;
    for(std::uint32_t n=0;n<state.physical_capacity;++n) {
        const auto& row=state.voices[n];
        if(row.key_marker==0x0ffeu || (row.key_marker>>8u)!=channel)continue;
        ++matches;
        if(!defer && row.release_sample_end>0x00fffffbu)
            return result; // missing original state: no partial chip writes
    }
    if(matches!=0u && !defer && !release_io_valid(state,io))return result;
    result.status=Status::Found;
    for(std::uint32_t n=0;n<state.physical_capacity;++n) {
        auto& row=state.voices[n];
        if(row.key_marker==0x0ffeu || (row.key_marker>>8u)!=channel)continue;
        ++result.matched;
        if(defer){
            row.key_marker=static_cast<std::uint16_t>(
                (static_cast<unsigned>(channel)<<8u)|0xffu);
            ++result.deferred;
        }else if(!release_one(row,static_cast<std::uint8_t>(n),io,result)){
            return result;
        }
    }
    return result;
}

Allocation allocate_mono_mode0(DeviceState const& state, PortIo io) noexcept {
    Allocation result;
    if(!valid(state,io))return result;
    result.status=Status::NoCandidate;
    Live live{io,result};
    std::uint32_t best=0xffffffffu;
    const auto capacity=static_cast<std::uint32_t>(state.physical_capacity);
    // Original LCOD+0x3EA1: first 0,3,6,..; then 1,4,7,..; then 2,5,8...
    // This ordering also governs equal-score tie replacement.
    for(std::uint32_t lane=0;lane<3u;++lane) {
        for(std::uint32_t n=lane;n<capacity;n+=3u) {
            const auto voice=static_cast<std::uint8_t>(n);
            const auto& row=state.voices[n];
            if(forbidden(row,state.mode_count))continue;
            if(row.key_marker<0x2000u) {
                std::uint32_t priority=0;
                if(!score(row,voice,live,priority)) {
                    result.status=Status::PortError;return result;
                }
                if(row.key_marker==0x0ffeu && priority==0u) {
                    result.first=voice;
                    result.priority=0;
                    result.immediate=true;
                    goto selected;
                }
                // Original +0x3F63..0x3F6E: score <= best updates winner.
                if(priority<=best) {
                    best=priority;
                    result.first=voice;
                    result.priority=priority;
                }
            }
            // Original +0x3F71 checks cached sample-end even for a
            // priority-ineligible key marker >=0x2000.
            bool finished=false;
            if(!sample_finished(row,voice,live,finished)) {
                result.status=Status::PortError;return result;
            }
            if(finished) {
                result.first=voice;
                result.priority=0;
                result.immediate=true;
                goto selected;
            }
        }
    }
selected:
    if(result.first==no_voice)return result;
    if(!live.release(result.first)) {
        result.status=Status::PortError;
        return result;
    }
    result.status=Status::Found;
    return result;
}

Allocation allocate_pair_mode1(DeviceState const& state, PortIo io) noexcept {
    Allocation result;
    if(!valid(state,io))return result;
    result.status=Status::NoCandidate;
    if(state.physical_capacity<2)return result;
    Live live{io,result};
    const auto decision=[&io](StereoDecisionKind kind, std::uint8_t voice) noexcept {
        if(io.stereo_decision_observer)
            io.stereo_decision_observer(io.stereo_decision_opaque,{kind,voice});
    };
    std::uint32_t previous=invalid_pair_score;
    std::uint32_t best=invalid_pair_score;
    const std::uint32_t count=state.mode_count;
    for(std::uint32_t n=state.physical_capacity; n>0; --n) {
        const std::uint8_t voice=static_cast<std::uint8_t>(n-1u);
        const auto& v=state.voices[voice];
        // Original SBAWE.VXD LCOD+3FFD..4048 (P003 owner V5):
        // ESI+18h <=2 activates these two 0x17 flag checks.
        // EBX points at voice row +0x0A: [EBX+0Dh] is row+0x17,
        // [EBX+2Dh] is row+0x37 = NEXT voice's row+0x17.
        // The upper neighbor exists only if n < original [ESI+06h].
        // Each skipped row follows +4111/+411A (EBX -= 0x20).
        // There is no fabricated +14/+36 cycle charge here.
        if(forbidden(v,count) ||
           (n<state.physical_capacity && forbidden(state.voices[n],count)) ||
           v.key_marker>=0x2000u) {
            decision(StereoDecisionKind::SkippedVoice,voice);
            previous=invalid_pair_score;
            continue;
        }
        std::uint32_t current=0;
        if(!score(v,voice,live,current)) {result.status=Status::PortError;return result;}
        decision(StereoDecisionKind::ScoredVoice,voice);
        bool finished=false;
        if(!sample_finished(v,voice,live,finished)) {result.status=Status::PortError;return result;}
        decision(v.sample_end==0u ? StereoDecisionKind::CursorNotRead :
                 finished ? StereoDecisionKind::CursorReadFinished :
                            StereoDecisionKind::CursorReadNotFinished,voice);
        if(finished) current=0;
        const std::uint32_t sum=previous+current;
        if(sum==0u) {
            // Real x86 jumps directly to +0x4167 then +0x4138.
            result.first=voice; result.second=static_cast<std::uint8_t>(n);
            result.priority=0; result.immediate=true;
            decision(StereoDecisionKind::ImmediatePair,voice);
            break;
        }
        const std::uint32_t priority=sum+2u*std::max(previous,current)+
                                     (v.pair_penalty?extra_pair_penalty:0u);
        const bool replaced=priority<=best;
        if(replaced) {
            best=priority;
            result.first=voice;
            result.second=static_cast<std::uint8_t>(n);
            result.priority=priority;
        }
        decision(replaced ? StereoDecisionKind::PriorityReplaced :
                            StereoDecisionKind::PriorityRetained,voice);
        previous=current;
    }
    if(result.first==no_voice || result.second>=state.physical_capacity) {
        result.first=no_voice; result.second=no_voice;
        result.status=Status::NoCandidate;
        return result;
    }
    // Writes are original +0x414A and +0x4158 (the two 16-bit releases).
    if(!live.release(result.first) || !live.release(result.second)) {
        result.status=Status::PortError;
        return result;
    }
    result.status=Status::Found;
    return result;
}

Allocation allocate_range_mode2(DeviceState const& state,
                                std::uint32_t packed_range,PortIo io) noexcept {
    Allocation result;
    if(!valid(state,io))return result;
    result.status=Status::NoCandidate;
    const std::uint32_t begin=packed_range&0xffu;
    const std::uint32_t end=std::min(packed_range>>8u,
                                      static_cast<std::uint32_t>(state.physical_capacity));
    if(begin>=end)return result;
    Live live{io,result};
    std::uint32_t best=0xffffffffu;
    for(std::uint32_t n=begin; n<end; ++n) {
        const std::uint8_t voice=static_cast<std::uint8_t>(n);
        const auto& v=state.voices[n];
        if(forbidden(v,state.mode_count))continue;
        if(v.key_marker<0x2000u) {
            std::uint32_t current=0;
            if(!score(v,voice,live,current)) {result.status=Status::PortError;return result;}
            if(v.key_marker==0x0ffeu && current==0u) {
                result.first=voice;result.priority=0;result.immediate=true;
                break;
            }
            if(current<=best) {
                best=current;result.first=voice;result.priority=current;
            }
        }
        bool finished=false;
        if(!sample_finished(v,voice,live,finished)) {result.status=Status::PortError;return result;}
        if(finished){result.first=voice;result.priority=0;result.immediate=true;break;}
    }
    if(result.first==no_voice)return result;
    if(!live.release(result.first)){result.status=Status::PortError;return result;}
    result.status=Status::Found;
    return result;
}
} // namespace awe64::original_vxd
