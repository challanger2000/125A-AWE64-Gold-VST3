#include "CreativeChipPortBridge.h"
#include <86box/snd_emu8k.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
using namespace awe64;
using namespace awe64::creative;
#define CHECK(expr) do{if(!(expr)){std::cerr<<"P002_REAL_CHIP_PORT_BRIDGE_FAIL line="<<__LINE__<<" "<<#expr<<"\n";return 1;}}while(0)
int main(){
    // Completely synthetic ROM. The chip itself is the actual locked
    // original 86Box-AWE32 source, not a synthesized test mock.
    const auto rom=std::filesystem::temp_directory_path()/
        "p002-noncreative-synthetic-chip-port-test.raw";
    {
        std::ofstream out(rom,std::ios::binary|std::ios::trunc);
        CHECK(bool(out));
        const std::array<char,4096> zero{};
        for(int i=0;i<256;++i)out.write(zero.data(),zero.size());
        CHECK(bool(out));
    }
    Chip chip;
    std::string err;
    CHECK(chip.open(rom,err));
    CHECK(chip.ready());
    CHECK(chip.rawForContractTesting()->ram_end_addr==0x1000000u);
    const auto selected=candidateOriginalWin95SelectedVoiceDcysusvWrites(
        OriginalWin95MonoSelection{
            3,OriginalWin95MonoSelectionReason::CompletelyIdle,0});
    const auto vtft=candidateOriginalWin95AssignedVoiceVtftWrites(3);
    CHECK(selected && vtft);
    std::array<OriginalWin95VxdPortWrite,4> start{
        (*selected)[0],(*selected)[1],(*vtft)[0],(*vtft)[1]};
    CHECK(applyOriginalCreativeVxdVoicePorts(chip,start));
    const auto* real=chip.rawForContractTesting();
    CHECK(real->voice[3].dcysusv==0x00ffu);
    CHECK(real->voice[3].vtft==0x0000ffffu);
    // Ensure no other physical voice was changed by the original prelude.
    CHECK(real->voice[4].dcysusv!=0x00ffu ||
          real->voice[4].vtft!=0x0000ffffu);
    // A private guest's real VXD 4AB5 group stop has 16-bit IFATN
    // then 16-bit DCYSUSV release phase; actual pin-locked chip must
    // reflect those register words, not merely an array equality test.
    const auto stop=candidateOriginalWin95VxdGroupChokeWrites(
        3,OriginalVxdGroupChokeEvidence{true,false,false});
    CHECK(stop && stop->count==4);
    CHECK(applyOriginalCreativeVxdVoicePorts(
        chip,std::span<const OriginalWin95VxdPortWrite>(
            stop->writes.data(),stop->count)));
    CHECK(real->voice[3].ifatn==0x00ffu);
    CHECK(real->voice[3].dcysusv==0x807fu);
    const auto stereo=candidateOriginalWin95VxdGroupChokeWrites(
        6,OriginalVxdGroupChokeEvidence{true,true,true});
    CHECK(stereo && stereo->count==8);
    CHECK(applyOriginalCreativeVxdVoicePorts(
        chip,std::span<const OriginalWin95VxdPortWrite>(
            stereo->writes.data(),stereo->count)));
    CHECK(real->voice[6].ifatn==0xffu && real->voice[7].ifatn==0xffu);
    CHECK(real->voice[6].dcysusv==0x807fu &&
          real->voice[7].dcysusv==0x807fu);
    const auto oldIfatn=real->voice[3].ifatn;
    // Malformed sequences must be rejected transactionally before writes.
    auto invalid=start;
    invalid[0].access_width_bits=16;
    CHECK(!applyOriginalCreativeVxdVoicePorts(chip,invalid));
    invalid=start;invalid[3].port=0xA20;
    CHECK(!applyOriginalCreativeVxdVoicePorts(chip,invalid));
    invalid=start;invalid[2].value=0x7eu; // voice30 reserved
    CHECK(!applyOriginalCreativeVxdVoicePorts(chip,invalid));
    invalid=start;invalid[1].value=0x10000u;
    CHECK(!applyOriginalCreativeVxdVoicePorts(chip,invalid));
    invalid=start;invalid[3].access_width_bits=8;
    CHECK(!applyOriginalCreativeVxdVoicePorts(chip,invalid));
    CHECK(real->voice[3].ifatn==oldIfatn);
    CHECK(!applyOriginalCreativeVxdVoicePorts(chip,{}));
    chip.close();
    CHECK(!applyOriginalCreativeVxdVoicePorts(chip,start));
    std::filesystem::remove(rom);
    std::cout<<"P002_ORIGINAL_VXD_PORTS_EXECUTED_ON_PINNED_EMU8000_PASS"
             <<" DCYSUSV=00FF VTFT=0000FFFF group_stop=807F"
             <<" stereo=TRUE zero_Creative_bytes=TRUE"
             <<" original_driver_PCM_parity=NOT_PROVEN\n";
    return 0;
}
