// Native P005 adapter regression using SYNTHETIC source/capture bytes.
// Never treat this as an original Win95 register/PCM attestation.
#include "P005OriginalCreativeHostAdapter.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
using awe64::vst3::P005OriginalCreativeHostAdapter;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"P003_HOST_OWNER_FAIL:%d %s\n",__LINE__,#x);std::abort();}}while(false)
namespace {
struct Chip {
    bool good=true;
    unsigned writes=0,reads=0;
    [[nodiscard]] bool ready() const noexcept{return good;}
    void writePort(std::uint16_t,std::uint16_t) noexcept{++writes;}
    [[nodiscard]] std::uint16_t readPort(std::uint16_t) noexcept{
        ++reads;return 0u;
    }
};
void word(std::span<std::uint8_t> d,std::size_t offset,unsigned x){
    d[offset]=static_cast<std::uint8_t>(x);
    d[offset+1]=static_cast<std::uint8_t>(x>>8u);
}
void dword(std::span<std::uint8_t> d,std::size_t offset,std::uint32_t x){
    word(d,offset,x);word(d,offset+2u,x>>16u);
}
}
int main(){
    using Adapter=P005OriginalCreativeHostAdapter<Chip>;
    Adapter host{};
    Chip chip{};
    std::array<VerifiedSf2Zone,1u> zones{};
    VerifiedSf2Bank bank{zones};
    auto& z=zones[0];
    z.original_zone_verified=true;
    z.voice.source_verified=true;
    z.voice.note_start_descriptor_verified=true;
    z.original_state.original_descriptor_verified=true;
    z.original_state.original_device_state_verified=true;
    z.original_state.original_sample_placement_verified=true;
    std::array<std::uint8_t,0x1592u> image{};
    std::array<std::uint8_t,180u> params{};
    std::array<std::uint16_t,128u> lookup{};
    word(image,6u,32u);
    dword(image,0x158eu,0x4d474d31u);
    for(std::size_t i=0u;i<32u;++i)
        word(image,0x42u+i*0x20u,0x0ffeu);
    params[6u]=9u;params[7u]=1u;
    params[8u]=0x60u;params[9u]=0x04u;
    const auto attach=[&](bool attestation) {
        return host.attachOriginalNrpnSourceNonRealtime(
            image,params,lookup,attestation,attestation,attestation);
    };
    CHECK(!attach(true)); // no active shared original chip and MIDI runtime
    CHECK(host.activate(chip,bank,true));
    CHECK(host.active()&&host.sourceBound());
    CHECK(!attach(false));
    CHECK(!host.originalNrpnSourceBound());
    // Captured guest device and shared native device do not match: reject.
    word(image,0x42u,0x0101u);
    CHECK(!attach(true));
    word(image,0x42u,0x0ffeu);
    CHECK(attach(true));
    CHECK(host.originalNrpnSourceBound());
    CHECK(!attach(true));
    auto* runtime=host.sharedRuntime();
    CHECK(runtime!=nullptr);
    CHECK(runtime->scratch.device==&runtime->midi.device);
    CHECK(runtime->attached_to(chip));
    CHECK(runtime->midi.original_nrpn_memory.device.data()!=image.data());
    CHECK(runtime->midi.original_nrpn_parameter_entries.data()!=params.data());
    CHECK(runtime->midi.original_nrpn_parameter_lookup.data()!=lookup.data());
    // External capture arrays can be modified: the owner has COPIED
    // original source rows/table into its own fixed-size stable storage.
    image[6u]=0u;params[6u]=0u;lookup[0u]=65535u;
    CHECK(runtime->midi.original_nrpn_memory.device[6u]==32u);
    CHECK(runtime->midi.original_nrpn_parameter_entries[6u]==9u);
    CHECK(runtime->midi.original_nrpn_parameter_lookup[0u]==0u);
    CHECK(host.controller(0u,99u,127u)==MidiResult::Applied);
    CHECK(host.controller(0u,98u,1u)==MidiResult::Applied);
    CHECK(host.controller(0u,6u,65u)==MidiResult::Applied);
    CHECK(runtime->midi.original_nrpn_memory.device[0x464u]==0x80u);
    CHECK(runtime->midi.original_nrpn_memory.device[0x465u]==0x20u);
    // Simulated NoteOn changes actual runtime voice ownership. Original
    // raw owner row remains stale: CC6 must not touch wrong voice.
    const auto before=chip.writes;
    runtime->midi.device.voices[0u].key_marker=60u;
    CHECK(host.controller(0u,6u,66u)==MidiResult::MissingSource);
    CHECK(chip.writes==before);
    CHECK(runtime->midi.original_nrpn_memory.device[0x464u]==0x80u);
    runtime->midi.device.voices[0u].key_marker=0x0ffeu;
    host.resetNonRealtime();
    CHECK(!host.sourceBound()&&!host.originalNrpnSourceBound());
    CHECK(host.sharedRuntime()==nullptr);
    CHECK(host.activate(chip,bank,true));
    CHECK(!host.originalNrpnSourceBound());
    CHECK(host.controller(0u,98u,1u)==MidiResult::MissingSource);
    host.resetNonRealtime();
    std::puts("P003_P005_HOST_NRPN_FIXED_OWNER_LIFETIME_PASS");
}
