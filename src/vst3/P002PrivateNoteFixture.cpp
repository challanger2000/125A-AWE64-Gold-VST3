#include "P002PrivateNoteFixture.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <utility>
namespace awe64::vst3 {
namespace {
constexpr std::array<std::uint8_t,8> MAGIC{'1','2','5','A','P','2','D','1'};
constexpr std::array<std::uint8_t,32> VXD_SHA{{
    0xd3,0x43,0xf2,0x42,0x8a,0xb3,0xe5,0x93,
    0xc9,0xe4,0x2a,0x66,0x8c,0xf5,0x21,0xb7,
    0x80,0x1e,0x12,0x39,0x79,0x2e,0xaf,0x7f,
    0x2b,0x11,0xb5,0xf1,0x79,0xbc,0x7c,0x9a
}};
constexpr std::array<std::uint8_t,32> BANK_SHA{{
    0x78,0xf8,0x73,0x9a,0x25,0xd4,0x9e,0x51,
    0x9b,0x3f,0xdc,0x92,0xa7,0xf0,0x3d,0x4d,
    0xdd,0x68,0x58,0x8f,0xd0,0x26,0x05,0x78,
    0xa8,0x80,0x29,0x6e,0x30,0x4b,0x7b,0x5f
}};
template<std::size_t N>
bool bytes(std::ifstream& f,std::array<std::uint8_t,N>& b) {
    return static_cast<bool>(f.read(reinterpret_cast<char*>(b.data()),N));
}
std::uint16_t le16(const std::uint8_t* b) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<unsigned>(b[0]) |
        (static_cast<unsigned>(b[1])<<8u));
}
}
bool loadP002PrivateNoteFixture(const std::filesystem::path& path,
                                P002PrivateNoteFixture& out,
                                std::string& error) {
    std::ifstream file(path,std::ios::binary);
    if(!file) {error="Private original P002 note fixture missing";return false;}
    std::array<std::uint8_t,8> magic{};
    std::array<std::uint8_t,6> header{};
    std::array<std::uint8_t,32> vxd{},bank{};
    if(!bytes(file,magic) || !bytes(file,header) ||
       !bytes(file,vxd) || !bytes(file,bank) ||
       magic!=MAGIC || (header[0]!=1u && header[0]!=2u &&
                         header[0]!=3u && header[0]!=4u && header[0]!=5u) ||
       vxd!=VXD_SHA ||
       bank!=BANK_SHA) {
        error="Private P002 diagnostic provenance/header invalid";return false;
    }
    if(header[1]>127u || header[2]>=16u || header[3]>127u ||
       header[4]==0u || header[4]>127u || header[5]==0u ||
       header[5]>awe64::creative::kOriginalCreativeMaxNoteStartDescriptors) {
        error="Private P002 diagnostic MIDI/voice header invalid";return false;
    }
    P002PrivateNoteFixture candidate{};
    candidate.program=header[1];candidate.channel=header[2];
    candidate.key=header[3];candidate.velocity=header[4];
    candidate.voice_count=header[5];
    candidate.verified_source_note_release=header[0]>=2u;
    candidate.two_note_diagnostic=header[0]>=4u;
    candidate.multi_note_diagnostic=header[0]==5u;
    const bool derive_release_from_original_note_start=header[0]>=3u;
    std::array<std::uint8_t,62> tables{};
    if(!bytes(file,candidate.original_device_state)||!bytes(file,tables)) {
        error="Private P002 diagnostic original state/tables truncated";return false;
    }
    for(unsigned i=0;i<15u;++i)
        candidate.negative_attenuation[i]=le16(tables.data()+i*2u);
    for(unsigned i=0;i<16u;++i)
        candidate.positive_attenuation[i]=le16(tables.data()+30u+i*2u);
    std::uint32_t seen=0;
    for(unsigned i=0;i<candidate.voice_count;++i) {
        char physical=0;
        if(!file.get(physical)) {
            error="Private P002 diagnostic physical voice missing";return false;
        }
        auto& destination=candidate.voices[i];
        auto& source=destination.assigned_voice;
        source.physical_voice=static_cast<std::uint8_t>(physical);
        source.played_midi_key=candidate.key;
        source.source_original_voice_verified=true;
        source.lfo={true,0,0}; // replaced by independently sourced channel state
        destination.original_midi_channel=candidate.channel;
        if(source.physical_voice>=awe64::creative::kOriginalCreativeMusicVoices||
           (seen&(1u<<source.physical_voice))!=0u ||
           !bytes(file,source.verified_after_start_record) ||
           source.verified_after_start_record[0x8cu]!=1u ||
           source.verified_after_start_record[0x8du]!=0u) {
            error="Private P002 diagnostic voice record invalid";return false;
        }
        if(candidate.verified_source_note_release &&
           !derive_release_from_original_note_start) {
            const std::size_t offset=
                awe64::creative::kOriginalVxdVoiceRowStart+
                static_cast<std::size_t>(source.physical_voice)*
                awe64::creative::kOriginalVxdVoiceRowStride;
            const auto& r=candidate.original_device_state;
            const auto note_marker=static_cast<std::uint16_t>(
                static_cast<unsigned>(r[offset])|
                (static_cast<unsigned>(r[offset+1u])<<8u));
            const auto expected=static_cast<std::uint16_t>(
                (static_cast<unsigned>(candidate.channel)<<8u)|candidate.key);
            const auto retained_end=static_cast<std::uint32_t>(r[offset+6u])|
                (static_cast<std::uint32_t>(r[offset+7u])<<8u)|
                (static_cast<std::uint32_t>(r[offset+8u])<<16u)|
                (static_cast<std::uint32_t>(r[offset+9u])<<24u);
            // Source LCOD+0x5A9A and +0x5AA3 copy raw low BYTES
            // from post-start generator30 and generator38, respectively.
            // A version2 caller cannot claim an unrelated (e.g. 92/45)
            // voice-record release rate for original GM0 (41/127).
            if(note_marker!=expected || retained_end>0xfffffbu ||
               r[offset+0x15u]!=source.verified_after_start_record[0x3cu] ||
               r[offset+0x16u]!=source.verified_after_start_record[0x4cu]) {
                error="Original P002 source NoteOff voice-record invalid";
                return false;
            }
        }
        seen|=1u<<source.physical_voice;
    }
    if(candidate.two_note_diagnostic) {
        // v4 suffix: second key, velocity, voice count; then for each
        // physical voice one BYTE index + original 148-byte descriptor.
        // A second distinct MIDI note is admitted only with an explicit,
        // source-validated physical voice assignment. No auto allocator.
        std::array<std::uint8_t,3> second{};
        if(!bytes(file,second) || second[0]>127u ||
           second[0]==candidate.key ||
           second[1]==0u || second[1]>127u ||
           second[2]==0u ||
           second[2]>awe64::creative::kOriginalCreativeMaxNoteStartDescriptors) {
            error="Private P002 second-note MIDI header invalid";return false;
        }
        candidate.second_key=second[0];
        candidate.second_velocity=second[1];
        candidate.second_voice_count=second[2];
        if(static_cast<unsigned>(candidate.voice_count)+
           static_cast<unsigned>(candidate.second_voice_count)>
           awe64::creative::kOriginalCreativeMusicVoices) {
            error="Private P002 physical note voice count exceeds chip";
            return false;
        }
        for(unsigned i=0;i<candidate.second_voice_count;++i) {
            char physical=0;
            if(!file.get(physical)) {
                error="Private P002 second-note physical voice truncated";
                return false;
            }
            auto& destination=candidate.second_voices[i];
            auto& voice=destination.assigned_voice;
            voice.physical_voice=static_cast<std::uint8_t>(physical);
            voice.played_midi_key=candidate.second_key;
            voice.source_original_voice_verified=true;
            voice.lfo={true,0,0};
            destination.original_midi_channel=candidate.channel;
            if(voice.physical_voice>=awe64::creative::kOriginalCreativeMusicVoices ||
               (seen&(1u<<voice.physical_voice))!=0u ||
               !bytes(file,voice.verified_after_start_record) ||
               voice.verified_after_start_record[0x8cu]!=1u ||
               voice.verified_after_start_record[0x8du]!=0u) {
                error="Private P002 second-note source/physical binding invalid";
                return false;
            }
            const auto& descriptor=voice.verified_after_start_record;
            const auto mode=static_cast<unsigned>(descriptor[108u])|
                (static_cast<unsigned>(descriptor[109u])<<8u);
            if(mode!=1u) {
                error="Private P002 second-note original release mode unverified";
                return false;
            }
            seen|=1u<<voice.physical_voice;
        }
    }
    if(candidate.multi_note_diagnostic) {
        char n=0;
        if(!file.get(n) || static_cast<std::uint8_t>(n)==0u ||
           static_cast<std::uint8_t>(n)>
               kP002MaxSourceNoteGroups-2u) {
            error="Private P002 v5 number of source note groups invalid";
            return false;
        }
        candidate.additional_note_count=static_cast<std::uint8_t>(n);
        std::uint32_t total_voices=
            static_cast<std::uint32_t>(candidate.voice_count)+
            candidate.second_voice_count;
        std::array<bool,128> used_keys{};
        used_keys[candidate.key]=true;
        used_keys[candidate.second_key]=true;
        for(unsigned group=0;group<candidate.additional_note_count;++group) {
            auto& note=candidate.additional_notes[group];
            std::array<std::uint8_t,3> midi{};
            if(!bytes(file,midi) || midi[0]>127u ||
               used_keys[midi[0]] || midi[1]==0u || midi[1]>127u ||
               midi[2]==0u ||
               midi[2]>awe64::creative::kOriginalCreativeMaxNoteStartDescriptors ||
               total_voices+midi[2]>awe64::creative::kOriginalCreativeMusicVoices) {
                error="Private P002 v5 MIDI group or physical voice budget invalid";
                return false;
            }
            note.key=midi[0];note.velocity=midi[1];
            note.voice_count=midi[2];total_voices+=note.voice_count;
            used_keys[note.key]=true;
            for(unsigned i=0;i<note.voice_count;++i) {
                char physical=0;
                if(!file.get(physical)) {
                    error="Private P002 v5 physical voice missing";
                    return false;
                }
                auto& assigned=note.voices[i];
                auto& voice=assigned.assigned_voice;
                voice.physical_voice=static_cast<std::uint8_t>(physical);
                voice.played_midi_key=note.key;
                voice.source_original_voice_verified=true;
                voice.lfo={true,0,0};
                assigned.original_midi_channel=candidate.channel;
                if(voice.physical_voice>=awe64::creative::kOriginalCreativeMusicVoices ||
                   (seen&(1u<<voice.physical_voice))!=0u ||
                   !bytes(file,voice.verified_after_start_record) ||
                   voice.verified_after_start_record[0x8cu]!=1u ||
                   voice.verified_after_start_record[0x8du]!=0u) {
                    error="Private P002 v5 descriptor or voice ownership invalid";
                    return false;
                }
                const auto& d=voice.verified_after_start_record;
                const auto mode=static_cast<unsigned>(d[108u])|
                    (static_cast<unsigned>(d[109u])<<8u);
                if(mode!=1u) {
                    error="Private P002 v5 source sample release mode invalid";
                    return false;
                }
                seen|=1u<<voice.physical_voice;
            }
        }
    }
    if(file.peek()!=std::char_traits<char>::eof()) {
        error="Private P002 diagnostic file has trailing data";return false;
    }
    if(derive_release_from_original_note_start) {
        // This v3 path proves the two VXD per-voice release bytes from
        // ORIGINAL SF2 NoteOn descriptor, not a synthesized static
        // per-program release preset. The retained sample END remains
        // separately supplied (zero in the verified ROM diagnostic mode).
        // Sample mode1 is the only proven no-retained-END branch here.
        for(const auto& voice:candidate.selectedVoices()) {
            const auto& desc=voice.assigned_voice.verified_after_start_record;
            const auto mode=static_cast<unsigned>(desc[2u*54u])|
                (static_cast<unsigned>(desc[2u*54u+1u])<<8u);
            if(mode!=1u) {
                error="Private P002 derived NoteOff sample mode unverified";
                return false;
            }
        }
        if(!awe64::creative::candidateOriginalSf2StoreNoteOnSourceReleaseState(
               candidate.original_device_state,true,candidate.selectedVoices())) {
            error="Private P002 original descriptor release state invalid";
            return false;
        }
    }
    // Fail closed by independently calculating the entire original
    // 61/122 OUT plan from this file, including the original source
    // VXD voice-IP write, before activation ever enters the audio thread.
    const auto source=candidate.selectedVoices();
    if(!awe64::creative::candidateOriginalSf2WriteInitialIpToVxdState(
           candidate.original_device_state,true,source) ||
       !awe64::creative::candidateOriginalSf2FullNoteOnFromVxdSnapshot(
           source,candidate.sourceState())) {
        error="Private P002 original-source descriptor/state mismatch";
        return false;
    }
    // Independently preflight EVERY later original NoteOn's complete
    // original register plan at activation, without marking its voices
    // active in the runtime state before receiving the MIDI NoteOn.
    // Entire v4/v5 fixture is rejected if ANY later source descriptor
    // fails, even one destined for the last physical voice.
    for(std::size_t group=1;group<candidate.noteGroupCount();++group) {
        auto laterState=candidate.original_device_state;
        const auto voices=candidate.groupVoices(group);
        if(!awe64::creative::candidateOriginalSf2StoreNoteOnSourceReleaseState(
               laterState,true,voices) ||
           !awe64::creative::candidateOriginalSf2WriteInitialIpToVxdState(
               laterState,true,voices)) {
            error="Private P002 additional source note-state invalid";
            return false;
        }
        auto check=candidate.sourceState();
        check.original_state_bytes=laterState;
        if(!awe64::creative::candidateOriginalSf2FullNoteOnFromVxdSnapshot(
               voices,check)) {
            error="Private P002 additional source note registers invalid";
            return false;
        }
    }
    out=std::move(candidate);
    error.clear();
    return true;
}
} // namespace awe64::vst3
