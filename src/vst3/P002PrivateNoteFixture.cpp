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
       magic!=MAGIC || header[0]!=1u || vxd!=VXD_SHA ||
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
        seen|=1u<<source.physical_voice;
    }
    if(file.peek()!=std::char_traits<char>::eof()) {
        error="Private P002 diagnostic file has trailing data";return false;
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
    out=std::move(candidate);
    error.clear();
    return true;
}
} // namespace awe64::vst3
