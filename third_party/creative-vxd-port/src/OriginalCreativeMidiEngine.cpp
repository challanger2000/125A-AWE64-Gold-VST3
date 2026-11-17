#include "OriginalCreativeMidiEngine.hpp"
#include "OriginalCreativePanController.hpp"
#include "OriginalCreativeVolumeController.hpp"
#include "OriginalCreativeModulationController.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

namespace awe64::original_vxd {
namespace {
struct RegisterPair {
    std::uint8_t selector=0; // EMU8000 selector WITHOUT voice index
    std::uint16_t port=0;
    std::uint8_t width=0;
};
// Original source has 18 + 16 + 22 physical OUT operations (28 pairs).
constexpr std::array<RegisterPair,original_note_main_data_registers> main_sequence{{
    {0x60u,0x620u,32},{0x40u,0x620u,32},
    {0x80u,0xa22u,16},{0xa0u,0xa22u,16},
    {0xc0u,0xa22u,16},{0xe0u,0xa20u,16},{0xe0u,0xa22u,16},
    {0x00u,0xe20u,16},{0x20u,0xe20u,16},
    {0x40u,0xe20u,16},{0x60u,0xe20u,16},{0x80u,0xe20u,16},
    {0xa0u,0xe20u,16},{0xc0u,0xa20u,16},{0x80u,0xa20u,16},
    {0x20u,0x620u,32},{0x00u,0x620u,32},
    {0xc0u,0x620u,32},{0xe0u,0x620u,32},{0x00u,0xa20u,32},
    {0xa0u,0x620u,32},{0x80u,0x620u,32},{0x00u,0xa20u,32},
    {0x60u,0x620u,32},{0x40u,0x620u,32},
    {0x20u,0x620u,32},{0x00u,0x620u,32},{0xa0u,0xa20u,16}
}};
static_assert(main_sequence.size()*2u==56u);
[[nodiscard]] constexpr std::uint16_t u16(Descriptor const& d,
                                            std::size_t at) noexcept {
    return static_cast<std::uint16_t>(static_cast<unsigned>(d[at])|
       (static_cast<unsigned>(d[at+1u])<<8u));
}
// Original LCOD+0x5B87..+0x5BE1, BEFORE the first note-start write.
// The allocator's cached end [+voice+0x0A] and the note-off release
// endpoint [+voice+0x06] are DISTINCT; only one may be nonzero.
struct OriginalNoteEndpoints {
    std::uint32_t allocator_end=0u;
    std::uint32_t release_end=0u;
    bool physical_class_verified=false;
    bool physical_ram=false;
};
[[nodiscard]] constexpr std::uint32_t u32(Descriptor const& d,
                                           std::size_t at) noexcept {
    return static_cast<std::uint32_t>(d[at]) |
        (static_cast<std::uint32_t>(d[at+1u])<<8u) |
        (static_cast<std::uint32_t>(d[at+2u])<<16u) |
        (static_cast<std::uint32_t>(d[at+3u])<<24u);
}
[[nodiscard]] std::optional<OriginalNoteEndpoints>
source_note_endpoints(SourceVoice const& source) noexcept {
    if(!source.note_start_descriptor_verified) {
        // Research-only caller may provide a separately proven release
        // endpoint. It cannot claim a nonloop cached-end without an
        // authenticated 148-byte post-start record.
        return OriginalNoteEndpoints{0u,source.release_end};
    }
    const auto& d=source.note_start_descriptor;
    const auto mode=u16(d,0x6cu);
    const auto start=u32(d,0x76u);
    const auto end=u32(d,0x7au);
    if(u16(d,0x8cu)!=1u || mode>3u ||
       start>=end || end>0x00fffffbu)
        return std::nullopt;
    // Nonloop: sample_end=end+4, release_end=0. Loop:
    // sample_end=0, release_end=end+4 ONLY for original mode bit 1.
    const bool ram_sample=start>=0x200000u;
    const OriginalNoteEndpoints answer=(mode&1u)!=0u
        ?OriginalNoteEndpoints{0u,(mode&2u)!=0u?end+4u:0u,true,ram_sample}
        :OriginalNoteEndpoints{end+4u,0u,true,ram_sample};
    // A nonzero externally supplied endpoint must never contradict the
    // actual original descriptor. Zero is the legacy 'not supplied'.
    if(source.release_end!=0u && source.release_end!=answer.release_end)
        return std::nullopt;
    return answer;
}
[[nodiscard]] bool valid_io(PortIo const io) noexcept {
    return io.out8 && io.out16 && io.out32 && io.in16 && io.in32;
}
[[nodiscard]] bool physical_reset(std::uint8_t voice,PortIo io) noexcept {
    // The allocator's VTFT read and DCYSUSV stop were already issued.
    // Finish the original allocation prologue: E22=(3<<5)|voice,
    // DATA0 VTFT=0000FFFF; no duplicate earlier selector/read.
    return io.out8(io.opaque,0xe22u,static_cast<std::uint8_t>(0x60u|voice)) &&
           io.out32(io.opaque,0x620u,0x0000ffffu);
}
[[nodiscard]] MidiResult result_of(Allocation const& a) noexcept {
    switch(a.status) {
        case Status::Found: return MidiResult::Applied;
        case Status::NoCandidate: return MidiResult::NoVoice;
        case Status::MissingSource: return MidiResult::MissingSource;
        case Status::PortError: return MidiResult::PortError;
    }
    return MidiResult::Invalid;
}
[[nodiscard]] bool preflight_program(std::uint8_t physical,
    OriginalVoiceProgram const& plan) noexcept {
    if(!plan.source_verified || physical>=physical_voices)return false;
    for(std::size_t i=0;i<main_sequence.size();++i) {
        if(main_sequence[i].width==16u && plan.data[i]>0xffffu)
            return false;
    }
    return true;
}
[[nodiscard]] bool write_main_range(std::uint8_t physical,
    OriginalVoiceProgram const& plan,PortIo io,
    std::size_t first,std::size_t last) noexcept {
    for(std::size_t i=first;i<last;++i) {
        const auto reg=main_sequence[i];
        const auto sel=static_cast<std::uint8_t>(reg.selector|physical);
        if(!io.out8(io.opaque,0xe22u,sel))return false;
        if(reg.width==16u) {
            if(!io.out16(io.opaque,reg.port,
                static_cast<std::uint16_t>(plan.data[i])))return false;
        } else {
            if(!io.out32(io.opaque,reg.port,plan.data[i]))return false;
        }
    }
    return true;
}
[[nodiscard]] MidiResult start_note(MidiEngine& eng,std::uint8_t ch,
    std::uint8_t key,std::uint8_t vel,PortIo io,SoundFontSource src) noexcept {
    if(!valid_io(io) || !src.resolve || !src.compile)
        return MidiResult::MissingSource;
    std::array<SourceVoice,max_source_note_layers> voices{};
    std::uint8_t count=0;
    // The original 1996 Program Change has a distinct drum (MIDI 10)
    // bank selection path at LCOD+0x6D55. Preserve that distinction when
    // passing the current state to the verified SF2 program directory.
    MidiChannel lookup=eng.channel[ch];
    lookup.original_channel_is_percussion=(ch==9u);
    if(!src.resolve(src.opaque,lookup,key,vel,voices,count) ||
       count==0u || count>max_source_note_layers)
        return MidiResult::MissingSource;
    std::array<OriginalNoteEndpoints,max_source_note_layers> endpoint_states{};
    for(unsigned i=0;i<count;++i) {
        const auto endpoints=source_note_endpoints(voices[i]);
        if(!endpoints)return MidiResult::MissingSource;
        endpoint_states[i]=*endpoints;
        if(!voices[i].source_verified ||
           voices[i].release_end>0x00fffffbu ||
           u16(voices[i].descriptor,0x8cu)==0u ||
           // already-encoded original sampleType mono=1, R=2, L=4.
           (u16(voices[i].descriptor,0x8cu)&~0x0007u)!=0u)
            return MidiResult::MissingSource;
    }
    std::array<Descriptor,max_source_note_layers> descriptors{};
    for(unsigned i=0;i<count;++i)descriptors[i]=voices[i].descriptor;
    const auto work=original_stereo_worklist(
        std::span<const Descriptor>(descriptors.data(),count));
    if(work.status!=WorkStatus::Ok || work.count!=count)
        return MidiResult::MissingSource;

    // The original uses the same allocator instance for every descriptor,
    // but a stereo left/right pair performs ONE paired allocation and the
    // right descriptor REUSES its previously selected physical voice.
    std::uint32_t reserved=0u;
    std::array<std::uint8_t,max_source_note_layers> started{};
    struct Pending {
        std::uint8_t physical=no_voice;
        std::uint8_t source_index=0;
    };
    std::array<Pending,max_source_note_layers> pending{};
    std::uint8_t pending_count=0;
    std::uint8_t started_count=0;
    for(std::size_t i=0;i<work.count;++i) {
        auto const& item=work.items[i];
        if(item.source_index>=count)return MidiResult::Invalid;
        const auto& entry=voices[item.source_index];
        std::uint8_t physical=no_voice;
        if(item.process_type==4u) {
            if(i+1u>=work.count || work.items[i+1u].process_type!=2u)
                return MidiResult::Invalid;
            Allocation a=allocate_pair_mode1(eng.device,io);
            if(a.status!=Status::Found){
                if(a.status==Status::PortError)eng.fault=true;
                return result_of(a);
            }
            if(a.first>=physical_voices-1u || a.second!=a.first+1u ||
               (reserved&(3u<<a.first))!=0u) {
                eng.fault=true;return MidiResult::PortError;
            }
            reserved|=3u<<a.first;
            // Port order is actual allocator stop/free pair then per-voice
            // VTFT 0000FFFF resets, BEFORE register programming.
            if(!physical_reset(a.first,io) || !physical_reset(a.second,io)) {
                eng.fault=true;return MidiResult::PortError;
            }
            physical=a.second; // partner processed first, source LCOD +0x5549
            // Persist the triggering-right voice for the next work item.
            started[started_count++]=a.first;
        } else if(item.process_type==2u) {
            if(started_count==0u || i==0u ||
               work.items[i-1u].process_type!=4u ||
               work.items[i-1u].paired_source!=item.source_index) {
                eng.fault=true;return MidiResult::Invalid;
            }
            physical=started[started_count-1u];
        } else if(item.process_type==1u) {
            Allocation a=allocate_mono_mode0(eng.device,io);
            if(a.status!=Status::Found){
                if(a.status==Status::PortError)eng.fault=true;
                return result_of(a);
            }
            physical=a.first;
            if(physical>=eng.device.physical_capacity ||
               (reserved&(1u<<physical))!=0u) {
                eng.fault=true;return MidiResult::PortError;
            }
            reserved|=1u<<physical;
            if(!physical_reset(physical,io)) {
                eng.fault=true;return MidiResult::PortError;
            }
        } else return MidiResult::Invalid;
        // Commit original allocation metadata BEFORE selecting the next
        // layer, even though we delay all voice-register programming.
        // This mirrors the observed original multi-layer sequence:
        //    all allocator prologues, then each voice's 56 main OUTs.
        auto& row=eng.device.voices[physical];
        // ORIGINAL LCOD+0x557D..+0x55A6 and LCOD+0x4C60..+0x4C8C:
        // a physical RAM start >=0x200000 clears voice+0x17 and
        // decrements device+0x18 ONLY if the flag had been set; ROM
        // starts set that flag and increment the count ONLY if clear.
        // The physical class is taken exclusively from an authenticated
        // note-start descriptor, never guessed from the MIDI program.
        const auto& endpoint=endpoint_states[item.source_index];
        if(endpoint.physical_class_verified) {
            if(endpoint.physical_ram && row.blocked) {
                if(eng.device.mode_count==0u){eng.fault=true;return MidiResult::Invalid;}
                --eng.device.mode_count;
                row.blocked=false;
            } else if(!endpoint.physical_ram && !row.blocked) {
                if(eng.device.mode_count==0xffffffffu){eng.fault=true;return MidiResult::Invalid;}
                ++eng.device.mode_count;
                row.blocked=true;
            }
        }
        row.key_marker=static_cast<std::uint16_t>((static_cast<unsigned>(ch)<<8u)|key);
        row.sample_end=endpoint_states[item.source_index].allocator_end;
        row.release_sample_end=endpoint_states[item.source_index].release_end;
        // Original release generators 38 (vol) and 30 (mod): encoded
        // release rates reside at +0x4c and +0x3c respectively.
        const auto& rates=entry.note_start_descriptor_verified
            ?entry.note_start_descriptor:entry.descriptor;
        row.volume_release=rates[0x4cu];
        row.modulation_release=rates[0x3cu];
        row.original_pitch_base=entry.original_pitch_base;
        row.original_pitch_base_verified=entry.original_pitch_base_verified;
        row.pan_state_verified=entry.pan_state_verified;
        row.original_pan_base=entry.original_pan_base;
        row.pan_target=entry.pan_target;
        row.pan_current=entry.pan_current;
        row.volume_terms_verified=entry.volume_terms_verified;
        row.original_velocity_volume_level=entry.original_velocity_volume_level;
        row.original_amplitude_word=entry.original_amplitude_word;
        row.original_modulation_word_verified=entry.original_modulation_word_verified;
        row.original_modulation_word_signed=entry.original_modulation_word_signed;
        pending[pending_count++]={physical,item.source_index};
    }
    // Verify ALL compiler-produced per-voice DATA words BEFORE executing
    // the first source note-main instruction (missing source is not audio).
    std::array<OriginalVoiceProgram,max_source_note_layers> programs{};
    for(std::size_t i=0;i<pending_count;++i) {
        const auto part=pending[i];
        if(!src.compile(src.opaque,voices[part.source_index],
                        part.physical,key,programs[i]) ||
           !preflight_program(part.physical,programs[i])) {
            eng.fault=true;
            return MidiResult::MissingSource;
        }
    }
    // ALL allocation prologues precede voice main programming.
    // Original Win95 then distinguishes two programming modes:
    //   MONO LAYERS: each 28-pair body triggers immediately before the
    //                next layer body (C4 0,3, D4 6,9).
    //   STEREO PAIR: partner (base+1) 27 pairs, right/base 27 pairs,
    //                THEN adjacent triggers base followed by base+1.
    // Verified against source V4 mono seq726191..726330 and all 25
    // paired original events (e.g. 1351768..1351913).
    for(std::size_t i=0;i<pending_count;++i) {
        if(work.items[i].process_type==4u) {
            if(i+1u>=pending_count || work.items[i+1u].process_type!=2u) {
                eng.fault=true;return MidiResult::Invalid;
            }
            if(!write_main_range(pending[i].physical,programs[i],io,0u,27u) ||
               !write_main_range(pending[i+1u].physical,programs[i+1u],io,0u,27u) ||
               !write_main_range(pending[i+1u].physical,programs[i+1u],io,27u,28u) ||
               !write_main_range(pending[i].physical,programs[i],io,27u,28u)) {
                eng.fault=true;return MidiResult::PortError;
            }
            ++i;
        } else {
            if(!write_main_range(pending[i].physical,programs[i],io,0u,28u)) {
                eng.fault=true;return MidiResult::PortError;
            }
        }
    }
    return MidiResult::Applied;
}
// Genuine LCOD+0x6F56 Win95 pitch bend: MSB64 is center; original
// integer scale ((msb-64)*128+lsb)*channel_range / 24, truncating
// toward zero with x86 signed IDIV. Store this delta in channel+0x14.
// For every hardware voice with marker-high-byte==MIDI channel, write
// DATA3/IP via original +0x3CB1 (E22 selector=voice, E20 word data).
// In particular: x86 does an UNSIGNED >0xffff saturation BEFORE its
// signed-negative check. A negative wrapped DWORD therefore becomes
// 0xffff (not 0) under the original instruction ordering.
[[nodiscard]] MidiResult original_pitchbend(MidiEngine& e,
    std::uint8_t ch,std::uint8_t lsb,std::uint8_t msb,PortIo io) noexcept {
    auto& channel=e.channel[ch];
    const auto signed14=(static_cast<std::int32_t>(msb)-64)*128+
                        static_cast<std::int32_t>(lsb);
    const auto range=channel.original_bend_range==0u?2u:
                         static_cast<unsigned>(channel.original_bend_range);
    const auto delta=(signed14*static_cast<std::int32_t>(range))/24;
    // Fail before source state mutation when a pitch update cannot be
    // evaluated from authentic per-voice baseline state.
    std::uint32_t matches=0;
    for(std::size_t i=0;i<e.device.physical_capacity;++i) {
        const auto& voice=e.device.voices[i];
        if((voice.key_marker>>8u)!=ch)continue;
        if(!voice.original_pitch_base_verified)return MidiResult::MissingSource;
        ++matches;
    }
    if(matches && (!io.out8 || !io.out16))return MidiResult::MissingSource;
    channel.original_bend_delta=delta;
    channel.pitch_bend=static_cast<std::uint16_t>(
        static_cast<unsigned>(lsb)|(static_cast<unsigned>(msb)<<7u));
    for(std::size_t i=0;i<e.device.physical_capacity;++i) {
        const auto& voice=e.device.voices[i];
        if((voice.key_marker>>8u)!=ch)continue;
        const auto total=static_cast<std::uint32_t>(delta)+
            static_cast<std::uint32_t>(static_cast<std::int32_t>(
                channel.original_pitch_tune))+
            static_cast<std::uint32_t>(voice.original_pitch_base);
        // Emulate x86 CMP unsigned/JA, then TEST signed/JGE exactly.
        std::uint32_t final=total>0xffffu?0xffffu:total;
        if(final<=0xffffu && static_cast<std::int32_t>(final)<0)final=0u;
        const auto sel=static_cast<std::uint8_t>(i);
        if(!io.out8(io.opaque,0xe22u,sel) ||
           !io.out16(io.opaque,0xe20u,static_cast<std::uint16_t>(final))) {
            e.fault=true;return MidiResult::PortError;
        }
    }
    return MidiResult::Applied;
}
// Original CC121 at LCOD+0x6C16 is NOT a generic GM reset. It clears
// +0x45E/+0x460/+0x462 and CC1, then calls CC64(0), CC11(127),
// pitch bend center, modulation(0), and optional original device callback.
// All known source inputs are checked first, so a missing VXD state never
// triggers a partial reset or any speculative hardware writes.
// Canonical owner image needed by LCOD+0x7309/0x740A/0x7350.
// This source view must be attached by the owner before audio processing,
// and must not alias recycled or truncated host memory.
[[nodiscard]] bool has_original_nrpn_callbacks(MidiEngine const& e) noexcept {
    return e.original_nrpn_callbacks_installed_verified &&
        e.original_nrpn_memory.original_source_authenticated &&
        e.original_nrpn_memory.device.size()>=
            research::nrpn_detail::kMinimumDevice;
}
[[nodiscard]] MidiResult original_nrpn_controller(MidiEngine& e,
    std::uint8_t channel,std::uint8_t controller,
    std::uint8_t value) noexcept {
    if(!has_original_nrpn_callbacks(e) ||
       e.original_reset_callback_absent_verified)
        return MidiResult::MissingSource;
    auto const result=(controller==98u)
        ?research::originalLcod7309(e.original_nrpn_memory,channel,value)
        :research::originalLcod740a(e.original_nrpn_memory,channel,value);
    if(!result)return MidiResult::MissingSource;
    const auto d=std::span<const std::uint8_t>(e.original_nrpn_memory.device);
    const auto row=static_cast<std::size_t>(channel)*0x24u;
    auto& c=e.channel[channel];
    c.original_rpn_mode=research::nrpn_detail::r16(d,0x45eu+row);
    c.original_rpn_aux_462=research::nrpn_detail::r16(d,0x462u+row);
    // In NRPN mode source channel+0x464 is the selected 16-bit parameter
    // value, rather than an RPN data-entry LSB. Do not truncate it.
    c.original_rpn_data_lsb=research::nrpn_detail::r16(d,0x464u+row);
    // Original EAX==1 for invalid selection and for a reset. This is
    // still a fully executed MIDI handler, not a port-failure condition.
    return MidiResult::Applied;
}
// LCOD+0x6B69/+0x6BE5: CC6/CC38 modify the genuine VXD channel
// WORD +0x464 and invoke the installed +0xB52 = LCOD+0x70E5 callback.
// A complete private device/table/voice mapping is mandatory: never
// invent register parameters or synthesize an NRPN fallback.
[[nodiscard]] bool has_original_nrpn_parameter_callback(
    MidiEngine const& e, std::uint8_t channel, PortIo const io) noexcept {
    if(!has_original_nrpn_callbacks(e) ||
       e.original_reset_callback_absent_verified ||
       !e.original_nrpn_parameter_callback_installed_verified ||
       !e.original_nrpn_parameter_tables_verified ||
       !e.original_nrpn_voice_rows_verified ||
       e.original_nrpn_parameter_entries.size()!=30u*6u ||
       e.original_nrpn_parameter_lookup.size()!=128u ||
       !io.out8 || !io.in16 || !io.out16)return false;
    const auto d=std::span<const std::uint8_t>(e.original_nrpn_memory.device);
    const auto off=static_cast<std::size_t>(channel)*0x24u;
    const auto voices=research::nrpn_detail::r16(d,6u);
    return voices<=physical_voices &&
           voices==e.device.physical_capacity &&
           research::nrpn_detail::r16(d,0x45eu+off)==0x0200u &&
           research::nrpn_detail::r16(d,0x462u+off)==
               e.channel[channel].original_rpn_aux_462;
}
[[nodiscard]] MidiResult original_nrpn_data_entry(
    MidiEngine& e, std::uint8_t channel, std::uint8_t controller,
    std::uint8_t value, PortIo io) noexcept {
    if(!has_original_nrpn_parameter_callback(e,channel,io))
        return MidiResult::MissingSource;
    auto& d=e.original_nrpn_memory.device;
    const auto off=0x464u+static_cast<std::size_t>(channel)*0x24u;
    const auto old=research::nrpn_detail::r16(d,off);
    // Source x86: CC6 (SHL SI,7; OR masked AX), CC38 (OR CX, AX).
    const auto next=static_cast<std::uint16_t>(controller==6u
        ? (static_cast<unsigned>(value)<<7u)|(old&0x007fu)
        : (old&0xff80u)|static_cast<unsigned>(value));
    research::nrpn_detail::w16(d,off,next);
    const research::Lcod70e5Inputs original{
        d, e.original_nrpn_parameter_entries,
        e.original_nrpn_parameter_lookup, true, io};
    const auto r=research::originalLcod70e5(original,channel);
    if(r==research::Lcod70e5Status::RejectedSource) {
        // Input provenance error: the callback has not emitted any port IO
        // or changed parameter records; restore the just-written data WORD.
        research::nrpn_detail::w16(d,off,old);
        return MidiResult::MissingSource;
    }
    // The original callback may return EAX=1 for an invalid parameter
    // select; that is a *handled MIDI message*, not a port error.
    e.channel[channel].original_rpn_data_lsb=next;
    if(r==research::Lcod70e5Status::PortError) {
        // The original writes its parameter state before chip IO, so
        // after a real partial OUT, never attempt a fictitious rollback.
        e.fault=true;
        return MidiResult::PortError;
    }
    return MidiResult::Applied;
}
[[nodiscard]] bool original_nrpn_rpn_preflight(MidiEngine const& e) noexcept {
    if(!e.original_nrpn_callbacks_installed_verified &&
       e.original_nrpn_memory.device.empty())return true;
    return has_original_nrpn_callbacks(e) &&
        !e.original_reset_callback_absent_verified;
}
// Keep original channel-row mirror consistent when CC100/101 changes
// mode or RPN selector after a previously source-bound CC98/99.
inline void original_rpn_write_to_owner(MidiEngine& e,
    std::uint8_t ch,bool clearData) noexcept {
    if(!has_original_nrpn_callbacks(e))return;
    const auto row=static_cast<std::size_t>(ch)*0x24u;
    auto& d=e.original_nrpn_memory.device;
    auto const& c=e.channel[ch];
    research::nrpn_detail::w16(d,0x45eu+row,c.original_rpn_mode);
    research::nrpn_detail::w16(d,0x460u+row,c.original_rpn_number);
    if(clearData)
        research::nrpn_detail::w16(d,0x464u+row,0u);
}
[[nodiscard]] MidiResult original_reset_controllers(MidiEngine& e,
    std::uint8_t ch,PortIo io) noexcept {
    const bool callbackInstalled=has_original_nrpn_callbacks(e);
    if(!e.original_reset_callback_absent_verified && !callbackInstalled)
        return MidiResult::MissingSource;
    // An attested NULL callback and an attested installed callback cannot
    // both describe the same original owner device state.
    if(e.original_reset_callback_absent_verified &&
       (e.original_nrpn_callbacks_installed_verified ||
        !e.original_nrpn_memory.device.empty()))
        return MidiResult::MissingSource;
    if(e.device.physical_capacity==0u ||
       e.device.physical_capacity>physical_voices)return MidiResult::Invalid;
    bool active=false;
    for(unsigned i=0u;i<e.device.physical_capacity;++i) {
        const auto& v=e.device.voices[i];
        if((v.key_marker>>8u)!=ch)continue;
        active=true;
        if(!v.original_pitch_base_verified || !v.original_modulation_word_verified ||
           !v.volume_terms_verified || v.release_sample_end>0x00fffffbu)
            return MidiResult::MissingSource;
    }
    if(active) {
        const auto& r=e.volume_source;
        if(!r.verified || !r.cc7_table_128 || !r.cc11_table_128 ||
           !r.device_additive_verified || !r.format1_verified ||
           !e.channel[ch].original_modulation_byte_44d_verified ||
           !io.out8 || !io.out16 || !io.in16 || !io.in32 || !io.out32)
            return MidiResult::MissingSource;
    }
    if(callbackInstalled) {
        // Original +0x6C2C..+0x6C3B clears owner channel's three
        // selectors BEFORE calling the optional +0xB5A callback.
        auto& d=e.original_nrpn_memory.device;
        const auto row=static_cast<std::size_t>(ch)*0x24u;
        research::nrpn_detail::w16(d,0x45eu+row,0u);
        research::nrpn_detail::w16(d,0x460u+row,0u);
        research::nrpn_detail::w16(d,0x462u+row,0u);
    }
    auto& c=e.channel[ch];
    c.original_rpn_mode=0u;
    c.original_rpn_number=0u;
    c.original_rpn_aux_462=0u;
    c.modulation=0u; // original first writes channel+0x44C=0
    const auto released=controller_sustain(e.device,ch,0u,io);
    if(released.status==Status::PortError){e.fault=true;return MidiResult::PortError;}
    if(released.status!=Status::Found)return MidiResult::MissingSource;
    if(active) {
        const auto volume=original_cc7_cc11_live(e,ch,11u,127u,io);
        if(volume!=MidiResult::Applied)return volume;
    } else {
        c.expression=127u;
    }
    const auto bend=original_pitchbend(e,ch,0u,64u,io);
    if(bend!=MidiResult::Applied)return bend;
    // Original +0x6F1E stores floor(0/30) at +0x44D before the
    // original +0x65D7 modulation hardware update.
    c.original_modulation_byte_44d=0u;
    c.original_modulation_byte_44d_verified=true;
    const auto modulation=original_cc1_modulation_live(e,ch,0u,io);
    if(modulation!=MidiResult::Applied)return modulation;
    if(callbackInstalled) {
        // The original CC121 invokes device+0xB5A with value 0 at the
        // end, which is precisely LCOD+0x740A, then +0x7350 resets
        // the 30 parameter flags/words and returns EAX=1.
        const auto result=research::originalLcod740a(
            e.original_nrpn_memory,ch,0u);
        if(!result)return MidiResult::MissingSource;
        const auto d=std::span<const std::uint8_t>(e.original_nrpn_memory.device);
        const auto row=static_cast<std::size_t>(ch)*0x24u;
        c.original_rpn_mode=research::nrpn_detail::r16(d,0x45eu+row);
        c.original_rpn_aux_462=research::nrpn_detail::r16(d,0x462u+row);
        c.original_rpn_data_lsb=research::nrpn_detail::r16(d,0x464u+row);
    }
    return MidiResult::Applied;
}
[[nodiscard]] MidiResult from_release(ReleaseResult const& r) noexcept {
    switch(r.status) {
        case Status::Found: return MidiResult::Applied;
        case Status::NoCandidate: return MidiResult::NoVoice;
        case Status::MissingSource: return MidiResult::MissingSource;
        case Status::PortError: return MidiResult::PortError;
    }
    return MidiResult::Invalid;
}
} // namespace

bool write_original_note_main(std::uint8_t physical,
                              OriginalVoiceProgram const& plan,
                              PortIo io) noexcept {
    if(!io.out8 || !io.out16 || !io.out32 ||
       !preflight_program(physical,plan))return false;
    return write_main_range(physical,plan,io,0u,28u);
}

MidiResult dispatch_midi(MidiEngine& eng,std::uint8_t status,
                         std::uint8_t d1,std::uint8_t d2,
                         PortIo io,SoundFontSource src) noexcept {
    if(eng.fault)return MidiResult::FaultLatched;
    if((status&0x80u)==0u || status>=0xf0u || d1>=128u || d2>=128u)
        return MidiResult::Invalid;
    const auto ch=static_cast<std::uint8_t>(status&0x0fu);
    const auto command=status&0xf0u;
    if(command==0x80u || (command==0x90u && d2==0u)) {
        const auto r=release_note(eng.device,ch,d1,io);
        if(r.status==Status::PortError)eng.fault=true;
        return from_release(r);
    }
    if(command==0x90u)return start_note(eng,ch,d1,d2,io,src);
    if(command==0xb0u) {
        auto& state=eng.channel[ch];
        switch(d1) {
            case 0u: state.bank_msb=d2;return MidiResult::Applied;
            case 98u: // original CC98 -> +0x67B9 -> B56 -> LCOD+0x7309
            case 99u: // original CC99 -> +0x67DB -> B5A -> LCOD+0x740A
                return original_nrpn_controller(eng,ch,d1,d2);
            case 100u: // original +0x6A35: RPN LSB selection
                if(!original_nrpn_rpn_preflight(eng))return MidiResult::MissingSource;
                state.original_rpn_mode=0x0100u;
                state.original_rpn_number=static_cast<std::uint16_t>(
                    (state.original_rpn_number&0xff00u)|d2);
                original_rpn_write_to_owner(eng,ch,false);
                return MidiResult::Applied;
            case 101u: // original +0x6A6C: RPN MSB clears data fine
                if(!original_nrpn_rpn_preflight(eng))return MidiResult::MissingSource;
                state.original_rpn_mode=0x0100u;
                state.original_rpn_number=static_cast<std::uint16_t>(
                    (state.original_rpn_number&0x00ffu)|
                    (static_cast<unsigned>(d2)<<8u));
                state.original_rpn_data_lsb=0u;
                original_rpn_write_to_owner(eng,ch,true);
                return MidiResult::Applied;
            case 38u: // original +0x6BA5
                // Original NRPN branch calls device+0xB52 (LCOD+0x70E5)
                // AFTER rewriting channel+0x464. Its complete parameter
                // source is not attached here, so refuse before mutation.
                if(state.original_rpn_mode==0x0200u)
                    return original_nrpn_data_entry(eng,ch,d1,d2,io);
                if(state.original_rpn_mode!=0x0100u)
                    return MidiResult::Unsupported;
                if(!original_nrpn_rpn_preflight(eng))
                    return MidiResult::MissingSource;
                state.original_rpn_data_lsb=d2;
                if(has_original_nrpn_callbacks(eng)) {
                    auto& d=eng.original_nrpn_memory.device;
                    const auto row=static_cast<std::size_t>(ch)*0x24u;
                    research::nrpn_detail::w16(d,0x464u+row,d2);
                }
                return MidiResult::Applied;
            case 6u: { // original +0x6AB1 Data Entry MSB
                // Original NRPN mode calls the installed LCOD+0x70E5
                // parameter writer. No source-verified 30x6 record/table
                // is bound to this MIDI runtime yet: fail closed.
                if(state.original_rpn_mode==0x0200u)
                    return original_nrpn_data_entry(eng,ch,d1,d2,io);
                if(state.original_rpn_mode!=0x0100u)
                    return MidiResult::Unsupported;
                if(!original_nrpn_rpn_preflight(eng))
                    return MidiResult::MissingSource;
                if(state.original_rpn_number==0u) {
                    state.original_bend_range=d2;
                    return MidiResult::Applied;
                }
                if(state.original_rpn_number==2u) {
                    const auto cents=std::clamp<int>(
                        static_cast<int>(d2)-64,-24,24)*100;
                    state.original_coarse_tune_cents=
                        static_cast<std::int16_t>(cents);
                    state.original_coarse_tune_verified=true;
                    return MidiResult::Applied;
                }
                if(state.original_rpn_number==1u) {
                    const auto units=(static_cast<std::uint32_t>(d2)<<7u)|
                        state.original_rpn_data_lsb;
                    const auto cents=(static_cast<std::int32_t>(units)-8192)*100/8192;
                    state.original_fine_tune_cents=
                        static_cast<std::int16_t>(cents);
                    state.original_fine_tune_verified=true;
                    return MidiResult::Applied;
                }
                return MidiResult::Unsupported;
            }
            case 32u: return MidiResult::Unsupported; // original +0x6682 does not dispatch CC32
            case 1u: return original_cc1_modulation_live(eng,ch,d2,io);
            case 7u: return original_cc7_cc11_live(eng,ch,d1,d2,io);
            case 10u: return original_channel_pan_cc10(eng,ch,d2,io); // original +0x68F3/0x6980 PSST/PTRX steps
            case 11u: return original_cc7_cc11_live(eng,ch,d1,d2,io);
            case 91u: state.reverb=static_cast<std::uint8_t>(
                (static_cast<unsigned>(d2)*90u)/100u);
                return MidiResult::Applied; // original +0x68A5 stores floor(90*v/100)
            case 93u: state.chorus=static_cast<std::uint8_t>(
                (static_cast<unsigned>(d2)*90u)/100u);
                return MidiResult::Applied; // LCOD+0x686B table[0] -> +0x68CC
            case 120u: // Original +0x681D -> +0x6C82 with flag=0
            case 123u: { // Original +0x6831 -> +0x6C82 flag=1
                const auto r=original_channel_all_notes_off(
                    eng.device,ch,d1==123u,io);
                if(r.status==Status::PortError)eng.fault=true;
                return from_release(r);
            }
            case 64u: {
                const auto r=controller_sustain(eng.device,ch,d2,io);
                if(r.status==Status::PortError)eng.fault=true;
                return from_release(r);
            }
            case 121u: // original dispatcher +0x6821 -> CC121 +0x6C16
                return original_reset_controllers(eng,ch,io);
            default: return MidiResult::Unsupported;
        }
    }
    if(command==0xc0u) {
        auto& state=eng.channel[ch];
        if(src.select_program) {
            auto request=state;
            request.original_channel_is_percussion=(ch==9u);
            OriginalProgramSelection choice{};
            if(!src.select_program(src.opaque,request,d1,choice) ||
               choice.status!=ProgramSelectionStatus::Found ||
               choice.resolved_program>255u) {
                // Original LCOD+0x6D26..+0x6D2D zeroes the selected
                // preset/slot/program fields BEFORE the C0 source lookup.
                // If it fails the previous loaded preset must not remain
                // playable; bank request stays intact for a later C0.
                state.original_program_selection_valid=false;
                state.original_selected_bank_slot=0u;
                state.original_selected_preset_row=0u;
                state.original_selected_bank=0u;
                state.original_selected_program=0u;
                state.program=d1;
                return MidiResult::MissingSource;
            }
            state.original_program_selection_valid=true;
            state.original_selected_bank_slot=choice.device_bank_slot;
            state.original_selected_preset_row=choice.original_preset_row;
            state.original_selected_bank=choice.resolved_bank;
            state.original_selected_program=static_cast<std::uint8_t>(
                choice.resolved_program);
        } else {
            state.original_program_selection_valid=false;
        }
        state.program=d1;
        return MidiResult::Applied;
    }
    if(command==0xe0u)return original_pitchbend(eng,ch,d1,d2,io);
    return MidiResult::Unsupported;
}

MidiResult feed_midi_byte(MidiEngine& eng,std::uint8_t byte,
                         PortIo io,SoundFontSource src) noexcept {
    if(eng.fault)return MidiResult::FaultLatched;
    if(byte>=0xf8u)return MidiResult::IgnoredRealtime;
    if(eng.sysex) {
        if(byte==0xf7u)eng.sysex=false;
        return MidiResult::Incomplete;
    }
    if(eng.system_remaining && (byte&0x80u)==0u) {
        --eng.system_remaining;
        return MidiResult::Incomplete;
    }
    if((byte&0x80u)!=0u) {
        eng.data_count=0;
        eng.message_status=0;
        eng.data_need=0;
        eng.system_remaining=0;
        if(byte>=0xf0u) {
            eng.running_status=0;
            if(byte==0xf0u)eng.sysex=true;
            if(byte==0xf1u || byte==0xf3u)eng.system_remaining=1u;
            if(byte==0xf2u)eng.system_remaining=2u;
            return MidiResult::Incomplete;
        }
        eng.message_status=byte;
        eng.running_status=byte;
        const auto command=byte&0xf0u;
        eng.data_need=(command==0xc0u || command==0xd0u)?1u:2u;
        return MidiResult::Incomplete;
    }
    if(eng.message_status==0u) {
        if(eng.running_status==0u)return MidiResult::Invalid;
        eng.message_status=eng.running_status;
        const auto command=eng.message_status&0xf0u;
        eng.data_need=(command==0xc0u || command==0xd0u)?1u:2u;
        eng.data_count=0;
    }
    if(eng.data_count>=2u)return MidiResult::Invalid;
    eng.data_bytes[eng.data_count++]=byte;
    if(eng.data_count<eng.data_need)return MidiResult::Incomplete;
    const auto status=eng.message_status;
    const auto first=eng.data_bytes[0];
    const auto second=static_cast<std::uint8_t>(
        eng.data_need==2u?eng.data_bytes[1]:0u);
    eng.data_count=0;
    eng.message_status=eng.running_status;
    return dispatch_midi(eng,status,first,second,io,src);
}
} // namespace awe64::original_vxd
