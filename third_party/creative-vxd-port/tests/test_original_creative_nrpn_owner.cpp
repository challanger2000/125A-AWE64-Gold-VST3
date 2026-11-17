#include "OriginalCreativeNrpnOwnedSource.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
using awe64::original_vxd::research::OriginalCreativeNrpnOwnedSource;
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"NRPN_OWNER_FAIL line %d: %s\n", __LINE__,#x);std::abort(); }}while(false)
namespace {
void wr(std::span<std::uint8_t> d,std::size_t p,unsigned x) noexcept {
 d[p]=static_cast<std::uint8_t>(x);
 d[p+1u]=static_cast<std::uint8_t>(x>>8u);
}
void wr32(std::span<std::uint8_t> d,std::size_t p,std::uint32_t x) noexcept {
 wr(d,p,x);wr(d,p+2u,x>>16u);
}
bool sel(void*,std::uint16_t,std::uint8_t) noexcept{return true;}
bool read(void*,std::uint16_t,std::uint16_t& v) noexcept{v=0;return true;}
bool write(void*,std::uint16_t,std::uint16_t) noexcept{return true;}
}
int main(){
 std::array<std::uint8_t,OriginalCreativeNrpnOwnedSource::kDeviceBytes> raw{};
 std::array<std::uint8_t,OriginalCreativeNrpnOwnedSource::kParameterBytes> table{};
 std::array<std::uint16_t,OriginalCreativeNrpnOwnedSource::kLookupWords> look{};
 wr(raw,6u,32u);
 wr32(raw,0x158eu,0x4d474d31u);
 for(std::size_t k=0u;k<32u;++k)wr(raw,0x42u+k*0x20u,0x0ffeu);
 table[6u]=9u;table[7u]=1u;table[8u]=0x60u;table[9u]=0x04u;
 constexpr std::uint8_t ch=2u;
 const auto base=0x24u*ch;
 wr(raw,0x45eu+base,0x0200u);
 wr(raw,0x462u+base,0x7f01u);
 wr(raw,0x464u+base,0x2015u);
 wr32(raw,0x442u+base+0x14u,0xffffff12u);
 MidiEngine e{};
 OriginalCreativeNrpnOwnedSource owner{};
 auto attach=[&](bool dev=true,bool param=true,bool voices=true){
   return owner.attachNonRealtime(e,raw,table,look,dev,param,voices);
 };
 // Every failure is transactional: no private source binding.
 REQUIRE(!attach(false));
 REQUIRE(!attach(true,false));
 REQUIRE(!attach(true,true,false));
 REQUIRE(!owner.attached());
 REQUIRE(e.original_nrpn_memory.device.empty());
 wr(raw,0x158eu,0u);
 REQUIRE(!attach());
 wr32(raw,0x158eu,0x4d474d31u);
 wr(raw,0x42u,0x1234u);
 REQUIRE(!attach());
 wr(raw,0x42u,0x0ffeu);
 wr(raw,0x45eu+base,0x0300u);
 REQUIRE(!attach());
 wr(raw,0x45eu+base,0x0200u);
 REQUIRE(owner.attachNonRealtime(e,raw,table,look,true,true,true));
 REQUIRE(owner.attached());
 REQUIRE(!attach()); // cannot attach while source is active.
 REQUIRE(e.original_nrpn_memory.device.data()!=raw.data());
 REQUIRE(e.original_nrpn_parameter_entries.data()!=table.data());
 REQUIRE(e.original_nrpn_parameter_lookup.data()!=look.data());
 REQUIRE(e.channel[ch].original_rpn_mode==0x0200u);
 REQUIRE(e.channel[ch].original_rpn_aux_462==0x7f01u);
 REQUIRE(e.channel[ch].original_rpn_data_lsb==0x2015u);
 REQUIRE(e.channel[ch].original_bend_delta==-238);
 // Changing/freeing source inputs later must not affect live owner.
 raw[6u]=0u;table[6u]=0u;look[0u]=0xfadeu;
 REQUIRE(e.original_nrpn_memory.device[6u]==32u);
 REQUIRE(e.original_nrpn_parameter_entries[6u]==9u);
 REQUIRE(e.original_nrpn_parameter_lookup[0u]==0u);
 const PortIo io{nullptr,&sel,&write,&read,nullptr,nullptr};
 const SoundFontSource src{};
 REQUIRE(dispatch_midi(e,0xb2u,99u,127u,io,src)==MidiResult::Applied);
 REQUIRE(dispatch_midi(e,0xb2u,98u,1u,io,src)==MidiResult::Applied);
 REQUIRE(dispatch_midi(e,0xb2u,6u,65u,io,src)==MidiResult::Applied);
 // Live channel row and owner device were mutated together; original
 // driver operated on the COPY, not the temporary caller buffers.
 REQUIRE(e.original_nrpn_memory.device[0x464u+base]==0x80u);
 REQUIRE(e.original_nrpn_memory.device[0x465u+base]==0x20u);
 REQUIRE(raw[0x464u+base]==0x15u);
 // When NoteOn changes the native voice rows without a mirror update,
 // the source callback must fail closed BEFORE register I/O.
 e.device.voices[0u].key_marker=0x0201u;
 REQUIRE(dispatch_midi(e,0xb2u,6u,66u,io,src)==MidiResult::MissingSource);
 e.device.voices[0u].key_marker=0x0ffeu;
 owner.detachNonRealtime();
 REQUIRE(!owner.attached());
 REQUIRE(e.original_nrpn_memory.device.empty());
 REQUIRE(e.original_nrpn_parameter_entries.empty());
 REQUIRE(e.original_nrpn_parameter_lookup.empty());
 REQUIRE(!e.original_nrpn_callbacks_installed_verified);
 REQUIRE(!e.original_nrpn_parameter_callback_installed_verified);
 REQUIRE(!e.original_nrpn_parameter_tables_verified);
 REQUIRE(!e.original_nrpn_voice_rows_verified);
 REQUIRE(dispatch_midi(e,0xb2u,98u,1u,io,src)==MidiResult::MissingSource);
 std::puts("P003_ORIGINAL_NRPN_OWNER_PRIVATE_BYTES_LIFETIME_AND_FAIL_CLOSED_PASS");
}
