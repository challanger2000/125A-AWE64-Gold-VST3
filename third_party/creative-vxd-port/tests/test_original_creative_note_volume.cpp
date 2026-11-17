// Synthetic arithmetic/provenance checks; original-x86 execution is separate.
#include "OriginalCreativeVolumeController.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"NOTE_VOLUME_FAIL %d %s\n",__LINE__,#x);std::exit(1);}}while(false)
int main() {
    std::array<std::uint8_t,128> velocity{},cc7{},cc11{};
    velocity[100]=10u;cc7[100]=64u;
    OriginalNoteVolumeResources source{};
    source.original_velocity_table_verified=true;
    source.velocity_table=velocity;
    source.controllers={true,cc7.data(),cc11.data(),10u,true,true,true};
    CHECK(original_note_attenuation(source,100u,24,100u,127u,true)==163u);
    CHECK(original_note_attenuation(source,100u,24,10u,127u,true)==55u);
    CHECK(original_note_attenuation(source,100u,-32768,100u,127u,true)==144u);
    CHECK(original_note_attenuation(source,100u,-37,100u,127u,true)==161u);
    CHECK(original_note_attenuation(source,100u,32767,100u,127u,true)==255u);
    // Source CC11 hard mute precedes the signed arithmetic/ROM adjustment.
    CHECK(original_note_attenuation(source,100u,-32768,127u,10u,true)==255u);
    source.controllers.format1=false;
    CHECK(original_note_attenuation(source,100u,24,100u,127u,true)==147u);
    source.controllers.format1=true;
    CHECK(original_note_attenuation(source,100u,24,100u,127u,false)==147u);
    // Missing source must not become an invented zero-attenuation voice.
    source.original_velocity_table_verified=false;
    CHECK(!original_note_attenuation(source,100u,24,100u,127u,false));
    source.original_velocity_table_verified=true;
    source.velocity_table=std::span<const std::uint8_t>(velocity.data(),127u);
    CHECK(!original_note_attenuation(source,100u,24,100u,127u,false));
    source.velocity_table=velocity;
    source.controllers.device_additive_verified=false;
    CHECK(!original_note_attenuation(source,100u,24,100u,127u,false));
    source.controllers.device_additive_verified=true;
    CHECK(!original_note_attenuation(source,0u,24,100u,127u,false));
    CHECK(!original_note_attenuation(source,128u,24,100u,127u,false));
    CHECK(!original_note_attenuation(source,100u,24,128u,127u,false));
    CHECK(!original_note_attenuation(source,100u,24,100u,128u,false));
    source.controllers.cc11_table_128=nullptr;
    CHECK(!original_note_attenuation(source,100u,24,100u,127u,false));
    std::puts("ORIGINAL_NOTE_VOLUME_SYNTHETIC_TEST_PASS");
}
