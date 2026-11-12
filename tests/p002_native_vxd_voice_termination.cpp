#include "CreativeVxdVoiceTermination.h"
#include <cstdint>
#include <iostream>
using namespace awe64::creative;
#define CHECK(cond) do {if(!(cond)) {std::cerr<<"P002_VXD_GROUP_CHOKE_FAIL line="<<__LINE__<<" "<<#cond<<"\n";return 1;}} while(0)
int main(){
    const auto missing=candidateOriginalWin95VxdGroupChokeWrites(
        3,OriginalVxdGroupChokeEvidence{});
    CHECK(!missing); // no caller-proven original channel/key/group match
    const auto mono=candidateOriginalWin95VxdGroupChokeWrites(
        3,OriginalVxdGroupChokeEvidence{true,false,false});
    CHECK(mono && mono->count==4);
    CHECK(mono->writes[0].port==0xE22 && mono->writes[0].value==0x23 &&
          mono->writes[0].access_width_bits==8);
    CHECK(mono->writes[1].port==0xE20 && mono->writes[1].value==0xFF &&
          mono->writes[1].access_width_bits==16);
    CHECK(mono->writes[2].port==0xE22 && mono->writes[2].value==0xA3 &&
          mono->writes[2].access_width_bits==8);
    CHECK(mono->writes[3].port==0xA20 && mono->writes[3].value==0x807F &&
          mono->writes[3].access_width_bits==16);
    const auto stereo=candidateOriginalWin95VxdGroupChokeWrites(
        6,OriginalVxdGroupChokeEvidence{true,true,true});
    CHECK(stereo && stereo->count==8);
    CHECK(stereo->writes[0].value==0x26 && stereo->writes[2].value==0xA6);
    CHECK(stereo->writes[4].value==0x27 && stereo->writes[6].value==0xA7);
    CHECK(stereo->writes[5].port==0xE20 && stereo->writes[5].value==0xFF);
    CHECK(stereo->writes[7].port==0xA20 && stereo->writes[7].value==0x807F);
    // Original VXD emits a stereo tail only when BOTH original flags pass.
    const auto oneFlag=candidateOriginalWin95VxdGroupChokeWrites(
        6,OriginalVxdGroupChokeEvidence{true,true,false});
    CHECK(oneFlag && oneFlag->count==4);
    const auto otherFlag=candidateOriginalWin95VxdGroupChokeWrites(
        6,OriginalVxdGroupChokeEvidence{true,false,true});
    CHECK(otherFlag && otherFlag->count==4);
    CHECK(!candidateOriginalWin95VxdGroupChokeWrites(
        29,OriginalVxdGroupChokeEvidence{true,true,true}));
    CHECK(candidateOriginalWin95VxdGroupChokeWrites(
        29,OriginalVxdGroupChokeEvidence{true,false,false})->count==4);
    CHECK(!candidateOriginalWin95VxdGroupChokeWrites(
        30,OriginalVxdGroupChokeEvidence{true,false,false}));
    CHECK(!candidateOriginalWin95VxdGroupChokeWrites(
        31,OriginalVxdGroupChokeEvidence{true,false,false}));
    CHECK(!candidateOriginalWin95VxdGroupChokeWrites(
        255,OriginalVxdGroupChokeEvidence{true,false,false}));
    std::cout<<"P002_ORIGINAL_SBAWE_VXD_FORCED_GROUP_CHOKE_PASS"
             <<" real_IFATN=00FF real_DCYSUSV=807F"
             <<" stereo_oracle_flags=REQUIRED"
             <<" no_fabricated_noteoff=TRUE\n";
}
