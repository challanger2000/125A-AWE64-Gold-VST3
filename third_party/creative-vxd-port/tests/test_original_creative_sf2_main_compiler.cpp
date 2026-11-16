#include "OriginalCreativeSf2MainCompiler.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <array>

using namespace awe64::original_vxd;
#define CHECK(v) do { if(!(v)){std::fprintf(stderr,"SF2_COMPILE_FAIL %d: %s\n",__LINE__,#v);std::exit(1);} } while(false)
void w16(Descriptor& d,std::size_t index,std::uint16_t v) {
    d[index*2u]=static_cast<std::uint8_t>(v);
    d[index*2u+1u]=static_cast<std::uint8_t>(v>>8u);
}
void w32(Descriptor& d,std::size_t off,std::uint32_t v) {
    for(unsigned i=0;i<4u;++i)d[off+i]=static_cast<std::uint8_t>(v>>(8u*i));
}
struct MockBus {
    std::uint8_t selector=0;
    std::array<std::uint32_t,28> data{};
    unsigned count=0;
    static bool out8(void* p,std::uint16_t port,std::uint8_t v) noexcept {
        auto& b=*static_cast<MockBus*>(p);
        if(port!=0xe22u || b.count>=28u)return false;
        b.selector=v;
        return true;
    }
    static bool out16(void* p,std::uint16_t,std::uint16_t val) noexcept {
        auto& b=*static_cast<MockBus*>(p);
        if(b.count>=28u)return false;
        b.data[b.count++]=val;return true;
    }
    static bool out32(void* p,std::uint16_t,std::uint32_t val) noexcept {
        auto& b=*static_cast<MockBus*>(p);
        if(b.count>=28u)return false;
        b.data[b.count++]=val;return true;
    }
    [[nodiscard]] PortIo io() noexcept {return {this,out8,out16,nullptr,nullptr,out32};}
};
int main() {
    SourceVoice s{};s.source_verified=true;
    auto& d=s.descriptor;
    // Fully source-encoded record, not raw SoundFont generator defaults.
    w16(d,0x8cu/2u,1u); // original normalized physical sample type
    w16(d,54u,1u); // looping
    w16(d,8u,180u);w16(d,9u,7u); // cutoff / Q
    w16(d,17u,64u);w16(d,15u,0u);w16(d,16u,0u);
    w16(d,18u,128u); // AUX send
    w16(d,19u,20u); // volume attenuation
    w16(d,34u,128u);w16(d,35u,0x007fu);w16(d,33u,0x8000u);
    w16(d,26u,128u);w16(d,27u,0x007fu);w16(d,25u,0x8000u);
    w16(d,28u,0x04u);w16(d,29u,0x7fu);
    w16(d,37u,0x05u);w16(d,36u,0x3fu);
    w16(d,21u,0x8000u);w16(d,23u,0x8000u);
    w16(d,22u,0xcdu);w16(d,24u,0xcdu);
    w16(d,5u,0u);w16(d,6u,0u);w16(d,7u,0u);
    w16(d,10u,0u);w16(d,11u,0x8du);w16(d,13u,0u);
    w16(d,51u,0u);w16(d,52u,0u);w16(d,56u,100u);
    w16(d,58u,0xffffu); // root-from-shdr
    d[0x8au]=60u;d[0x8bu]=0u;
    w32(d,0x86u,0xe00000u);
    w32(d,0x76u,0x1000u);w32(d,0x7au,0x2000u);
    w32(d,0x7eu,0x1100u);w32(d,0x82u,0x1f00u);
    OriginalVoiceStartSources v{};
    v.original_descriptor_verified=true;
    v.original_device_state_verified=true;
    v.original_sample_placement_verified=true;
    v.original_voice_volume_target=0x1234u;
    v.original_pitch_target=0x8000u;
    OriginalVoiceProgram program{};
    CHECK(compile_original_sf2_main(s,30u,60u,v,program));
    CHECK(program.source_verified);
    CHECK(program.data[0]==0xffffu && program.data[1]==0xffffu);
    CHECK(program.data[2]==0x7f7fu && program.data[4]==0x7f7fu);
    CHECK(program.data[7]==0xe000u);
    CHECK(program.data[8]==0xb414u);
    CHECK(program.data[9]==0x008du);
    CHECK(program.data[13]==0xbfffu && program.data[14]==0xbfffu);
    CHECK(program.data[17]==(0x40u<<24u | 0x10ffu));
    CHECK(program.data[18]==0x1effu);
    CHECK(program.data[19]==0x0ffcu);
    CHECK(program.data[22]==0x70000ffcu);
    CHECK(program.data[23]==0x1234b400u && program.data[24]==program.data[23]);
    CHECK(program.data[25]==0x80000080u && program.data[26]==0x80000000u);
    CHECK(program.data[27]==0x053fu);
    MockBus b{};
    CHECK(write_original_note_main(30u,program,b.io()));
    CHECK(b.count==28u && b.data==program.data);
    // Original exact VXD root-key delta 100 cents, scale 874 -> note61.
    CHECK(compile_original_sf2_main(s,30u,61u,v,program));
    CHECK(program.data[7]==static_cast<std::uint16_t>((0xe00000u+87400u)>>8u));
    // Zero DRY effect sends and source non-looping geometry.
    w16(d,54u,0u);v.original_voice_volume_target=0x1234u;
    CHECK(compile_original_sf2_main(s,0u,60u,v,program));
    CHECK(program.data[17]==(0x40u<<24u | 0x2004u));
    CHECK(program.data[18]==0x2008u);
    // Private 1996 LCOD +0x5B87 branches on sample mode BEFORE reading
    // loopStart/loopEnd. A valid SF2 guard-end=sampleEnd+2 must not be
    // mistaken for corruption, and nonlooping uses end+4/end+8 only.
    w32(d,0x82u,0x2002u);
    CHECK(compile_original_sf2_main(s,0u,60u,v,program));
    CHECK(program.data[17]==(0x40u<<24u|0x2004u));
    CHECK(program.data[18]==0x2008u);
    w16(d,54u,1u);
    CHECK(compile_original_sf2_main(s,0u,60u,v,program));
    CHECK(program.data[17]==(0x40u<<24u|0x10ffu));
    CHECK(program.data[18]==0x2001u);
    w32(d,0x82u,0x2003u);
    CHECK(!compile_original_sf2_main(s,0u,60u,v,program));
    w32(d,0x82u,0x1f00u);w16(d,54u,0u);
    // Invalid externally supplied driver state or sample geometry fails
    // before any accidental SOUND FONT fallback/fake program values.
    v.original_descriptor_verified=false;
    CHECK(!compile_original_sf2_main(s,0u,60u,v,program));
    v.original_descriptor_verified=true;
    w16(d,0x8cu/2u,2u);
    CHECK(!compile_original_sf2_main(s,0u,60u,v,program));
    w16(d,0x8cu/2u,1u);
    w16(d,9u,16u);
    CHECK(!compile_original_sf2_main(s,0u,60u,v,program));
    w16(d,9u,7u);
    v.original_ram_sample=true;
    CHECK(!compile_original_sf2_main(s,30u,60u,v,program));
    w16(d,54u,1u);
    w32(d,0x76u,0x200100u);w32(d,0x7au,0x200300u);
    w32(d,0x7eu,0x200180u);w32(d,0x82u,0x2002a0u);
    CHECK(compile_original_sf2_main(s,30u,60u,v,program));
    CHECK(program.data[17]==(0x40u<<24u | 0x20017fu));
    CHECK(program.data[18]==0x20029fu);
    CHECK(program.data[19]==0x2000fcu);
    // Bad original physical RAM address range: source refuses.
    w32(d,0x76u,0x1ff000u);
    CHECK(!compile_original_sf2_main(s,30u,60u,v,program));
    std::puts("P003_PORTABLE_CREATIVE_SF2_28_REGISTER_COMPILER_TEST_PASS");
}
