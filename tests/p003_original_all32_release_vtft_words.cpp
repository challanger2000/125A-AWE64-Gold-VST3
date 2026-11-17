// P003: exhaustive output-word contract for all 32 original mode0 voices.
// The two actual Win95 original CPU I/O captures independently prove
// 11,756/11,756 releases with this exact hardware transaction sequence:
// (E22 BYTE 0xA0|v), (A20 WORD 0x00FF), (E22 BYTE 0x60|v),
// (DATA0 DWORD 0x0000FFFF -> physical 620 WORD FFFF and 622 WORD 0000).
// This is an independent *source-level* C++23 check of the two helpers;
// it does NOT claim they have yet been called by the product VST3 host.
#include "CreativeVxdMonoVoiceRanking.h"
#include <array>
#include <cstdint>
#include <cstdio>

namespace ac=awe64::creative;
int main() {
    using R=ac::OriginalWin95MonoSelectionReason;
    constexpr std::array<R,3> reasons{
        R::CompletelyIdle,R::SampleEndReached,R::LowestEnvelopePriority
    };
    for(std::uint8_t voice=0u;voice<32u;++voice) {
        const auto vtft=ac::candidateOriginalWin95AssignedVoiceVtftWrites(voice);
        if(!vtft ||
           (*vtft)[0].port!=0x0e22u ||
           (*vtft)[0].value!=(0x60u|voice) ||
           (*vtft)[0].access_width_bits!=8u ||
           (*vtft)[1].port!=0x0620u ||
           (*vtft)[1].value!=0x0000ffffu ||
           (*vtft)[1].access_width_bits!=32u ||
           ((*vtft)[1].value&0xffffu)!=0xffffu ||
           (((*vtft)[1].value>>16u)&0xffffu)!=0u) {
            std::printf("P003_MODE0_ALL32_VTFT_FAILED voice=%u\n",voice);
            return 1;
        }
        for(const auto reason:reasons) {
            const ac::OriginalWin95MonoSelection selected{voice,reason,0u};
            const auto release=ac::candidateOriginalWin95SelectedVoiceDcysusvWrites(selected);
            if(!release ||
               (*release)[0].port!=0x0e22u ||
               (*release)[0].value!=(0xa0u|voice) ||
               (*release)[0].access_width_bits!=8u ||
               (*release)[1].port!=0x0a20u ||
               (*release)[1].value!=0x00ffu ||
               (*release)[1].access_width_bits!=16u) {
                std::printf("P003_MODE0_ALL32_RELEASE_FAILED voice=%u\n",voice);
                return 2;
            }
        }
    }
    for(const std::uint8_t invalid: {std::uint8_t(32),std::uint8_t(255)}) {
        if(ac::candidateOriginalWin95AssignedVoiceVtftWrites(invalid) ||
           ac::candidateOriginalWin95SelectedVoiceDcysusvWrites(
               ac::OriginalWin95MonoSelection{
                   invalid,R::CompletelyIdle,0u})) {
            std::puts("P003_MODE0_OUT_OF_RANGE_VOICE_ACCEPTED");
            return 3;
        }
    }
    std::puts("P003_ALL_32_VOICE_RELEASE_VTFT_SOURCE_WORDS_PASS"
              " physical_voices=32 selection_reasons=3"
              " verified_source_read_only=YES product_vst3_parity=NOT_PROVEN");
    return 0;
}
