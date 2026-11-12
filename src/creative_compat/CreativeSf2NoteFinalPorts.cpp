#include "CreativeSf2NoteFinalPorts.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace awe64::creative {
namespace {
std::uint16_t word(const CreativeSf2VoiceDescriptorBytes& b,
                   const unsigned op) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<unsigned>(b[2u*op]) |
        static_cast<unsigned>(b[2u*op+1u])<<8u);
}
std::uint32_t dword(const CreativeSf2VoiceDescriptorBytes& b,
                    const unsigned offset) noexcept {
    return static_cast<std::uint32_t>(b[offset]) |
        (static_cast<std::uint32_t>(b[offset+1u])<<8u) |
        (static_cast<std::uint32_t>(b[offset+2u])<<16u) |
        (static_cast<std::uint32_t>(b[offset+3u])<<24u);
}
} // namespace

std::optional<
    std::array<OriginalWin95VxdPortWrite,kOriginalSf2FinalNoteStartPortCount>>
candidateOriginalSf2FinalNoteStartPorts(
    const CreativeSf2VoiceDescriptorBytes& after,
    const std::uint8_t voice,
    const OriginalSf2VerifiedFinalTargetState& verified) noexcept {
    if(!verified.original_driver_state_verified ||
       voice>=kOriginalCreativeMusicVoices ||
       word(after,0x8cu/2u)!=0x0001u)
        return std::nullopt;
    // Source LCOD+0x5B81 prepares addresses FROM the true SF2 shdr.
    SbkRomAddress sample{};
    sample.source_format=OriginalCreativeBankFormat::Format2Sf2;
    sample.sample_id=word(after,53u);
    sample.start_word=dword(after,0x76u);
    sample.end_word=dword(after,0x7au);
    sample.loop_start_word=dword(after,0x7eu);
    sample.loop_end_word=dword(after,0x82u);
    sample.sample_type=0x8001u; // original pre-start ROM-only qualifier

    // Original note record g54 is a MODE WORD. Only the two LCOD+0x5B87
    // checked bits are modeled. No unverified RAM / stereo start path.
    const auto mode=word(after,54u);
    const auto filterQ=word(after,9u);
    const auto pan=word(after,17u);
    const auto chorus=word(after,15u);
    const auto reverb=word(after,16u);
    const auto aux=word(after,18u);
    const auto volDecay=word(after,36u);
    const auto volSustain=word(after,37u);
    const auto cutoff=word(after,8u);
    if(mode>3u || filterQ>15u || pan>255u || chorus>255u ||
       reverb>255u || aux>255u || cutoff>255u ||
       volDecay>255u || volSustain>255u)
        return std::nullopt;
    // SOURCE LCOD+0x5DCD sets the high VTFT word to zero unless the
    // encoded source volume attack rate is exactly 128 AND its delay
    // is >=0x8000. For that exceptional branch, high16 is the original
    // VXD device-state-derived target and must be a VERIFIED input.
    if((word(after,34u)!=128u || word(after,33u)<0x8000u) &&
       verified.original_voice_volume_target!=0u)
        return std::nullopt;

    OriginalVxdRomNoteStartMode m{};
    m.original_driver_mode_verified=true;
    m.encoded_mode_flags=static_cast<std::uint8_t>(mode);
    m.original_pan_byte=static_cast<std::uint8_t>(pan);
    m.original_chorus_send_byte=static_cast<std::uint8_t>(chorus);
    const auto geometry=candidateOriginalVxdInitialRomNoteGeometry(
        sample,voice,m);
    const auto unfiltered=candidateOriginalVxdRomCccaStart(
        sample,voice,static_cast<std::uint8_t>(filterQ),true,
        OriginalVxdCccaStartPhase::StartWithoutFilterQ);
    const auto withQ=candidateOriginalVxdRomCccaStart(
        sample,voice,static_cast<std::uint8_t>(filterQ),true,
        OriginalVxdCccaStartPhase::StartWithEncodedFilterQ);
    if(!geometry || !unfiltered || !withQ)return std::nullopt;
    const auto ptr=[voice](const std::uint8_t reg) noexcept {
        return OriginalWin95VxdPortWrite{
            0xe22u,static_cast<std::uint32_t>((reg<<5u)|voice),8u};
    };
    const auto d32=[](const std::uint16_t port,const std::uint32_t v) noexcept {
        return OriginalWin95VxdPortWrite{port,v,32u};
    };
    const auto last=static_cast<std::uint16_t>((volSustain<<8u)|volDecay);
    const auto finalFilterVol=(
        static_cast<std::uint32_t>(verified.original_voice_volume_target)<<16u)|
        (static_cast<std::uint32_t>(cutoff)<<8u);
    const auto pitchTarget=
        static_cast<std::uint32_t>(verified.original_pitch_target)<<16u;
    const auto ptrx=pitchTarget|((static_cast<std::uint32_t>(reverb)<<8u)|aux);

    // Exactly source x86 at LCOD+0x5EEB..0x6021. The two early sample
    // cursor writes are taken from the original ROM geometry helper; all
    // following writes are independent source-derived register operations.
    return std::array<OriginalWin95VxdPortWrite,
                      kOriginalSf2FinalNoteStartPortCount>{{
        geometry->loop_address_ports[0], geometry->loop_address_ports[1],
        geometry->loop_address_ports[2], geometry->loop_address_ports[3],
        (*unfiltered)[0],(*unfiltered)[1],
        ptr(5u),d32(0x620u,0u),      // Z1 = 0
        ptr(4u),d32(0x620u,0u),      // Z2 = 0
        (*withQ)[0],(*withQ)[1],     // CCCA with encoded filterQ
        ptr(3u),d32(0x620u,finalFilterVol), // VTFT
        ptr(2u),d32(0x620u,finalFilterVol), // CVCF
        ptr(1u),d32(0x620u,ptrx),  // PTRX pitch target/reverb/aux
        ptr(0u),d32(0x620u,pitchTarget), // CPF pitch target
        ptr(5u),OriginalWin95VxdPortWrite{0xa20u,last,16u} // trigger
    }};
}
} // namespace awe64::creative
