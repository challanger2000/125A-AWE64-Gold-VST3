// P003 strict *actual SoundFont PHDR* inventory regression.
// Synthetic RIFF records only: no proprietary ROM, SF2 or VXD.
#include "CreativeSf2PresetInventory.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>
#include <vector>
using namespace awe64::creative;
using Bytes=std::vector<std::uint8_t>;
#define CHECK(x) do {if(!(x)) {std::cerr<<"P003_PHDR_INVENTORY_FAIL at "<<__LINE__<<" condition "<<#x<<"\n";return 1;}}while(false)
namespace {
void put16(Bytes& a,std::uint16_t n){a.push_back(n&255);a.push_back(n>>8);}
void put32(Bytes& a,std::uint32_t n){put16(a,n&65535);put16(a,n>>16);}
void set16(Bytes& a,std::size_t p,std::uint16_t v){a.at(p)=v&255;a.at(p+1)=v>>8;}
void set32(Bytes& a,std::size_t p,std::uint32_t v){set16(a,p,v&65535);set16(a,p+2,v>>16);}
void chunk(Bytes& a,std::string_view label,const Bytes& data){
    a.insert(a.end(),label.begin(),label.end());
    put32(a,static_cast<std::uint32_t>(data.size()));
    a.insert(a.end(),data.begin(),data.end());
    if(data.size()&1u)a.push_back(0u);
}
void header(Bytes& a,std::string_view name,
            std::uint16_t program,std::uint16_t bank,
            std::uint16_t bag) {
    std::array<char,20> raw{};
    std::memcpy(raw.data(),name.data(),name.size());
    a.insert(a.end(),raw.begin(),raw.end());
    put16(a,program);put16(a,bank);put16(a,bag);
    put32(a,0u);put32(a,0u);put32(a,0u);
}
struct Sample {
    Bytes data;
    std::size_t phdr=0,pbag=0;
};
Sample make(){
    Bytes phdr,pbag,pdta,root;
    header(phdr,"Grand Piano",0,0,0);
    header(phdr,"Lead 77",77,0,1);
    header(phdr,"Drum Kit",2,128,2);
    header(phdr,"EOP",0,0,3);
    for(unsigned i=0;i<4u;++i){put16(pbag,i);put16(pbag,0);}
    pdta.insert(pdta.end(),{'p','d','t','a'});
    chunk(pdta,"phdr",phdr);
    chunk(pdta,"pbag",pbag);
    root.insert(root.end(),{'s','f','b','k'});
    chunk(root,"LIST",pdta);
    Bytes total{'R','I','F','F'};
    put32(total,static_cast<std::uint32_t>(root.size()));
    total.insert(total.end(),root.begin(),root.end());
    // RIFF 12 bytes; LIST 8; LIST 'pdta' 4; phdr header 8.
    const auto phdrStart=12u+8u+4u+8u;
    const auto pbagStart=phdrStart+phdr.size()+8u;
    return {total,phdrStart,pbagStart};
}
}
Sample makeSparse77(){
    Bytes phdr,pbag,pdta,root;
    std::uint16_t row=0;
    for(std::uint16_t program=0;program<=77;++program){
        if(program==63u)continue;
        header(phdr,"SYNTH-PRESET",program,0u,row++);
    }
    header(phdr,"EOP",255u,255u,row);
    for(std::uint16_t n=0;n<=row;++n){put16(pbag,n);put16(pbag,0);}
    pdta.insert(pdta.end(),{'p','d','t','a'});
    chunk(pdta,"phdr",phdr);
    chunk(pdta,"pbag",pbag);
    root.insert(root.end(),{'s','f','b','k'});
    chunk(root,"LIST",pdta);
    Bytes riff{'R','I','F','F'};
    put32(riff,static_cast<std::uint32_t>(root.size()));
    riff.insert(riff.end(),root.begin(),root.end());
    return {riff,32u,32u+phdr.size()+8u};
}
int main(){
    auto f=make();
    const auto good=inspectOriginalSoundFontPresetInventory(f.data);
    CHECK(good && good->size()==3u); // NOT 128 padded.
    CHECK((*good)[0].bank==0u && (*good)[0].program==0u);
    CHECK((*good)[1].bank==0u && (*good)[1].program==77u);
    CHECK((*good)[2].bank==128u && (*good)[2].program==2u);
    CHECK((*good)[1].original_phdr_row==1u &&
          (*good)[2].first_pbag==2u &&
          (*good)[2].last_pbag==3u);
    CHECK(std::string_view((*good)[1].name.data())=="Lead 77");
    CHECK(hasExactSoundFontPreset(*good,0,77));
    CHECK(hasExactSoundFontPreset(*good,128,2));
    CHECK(!hasExactSoundFontPreset(*good,0,2));
    CHECK(!hasExactSoundFontPreset(*good,128,77));
    CHECK(!hasExactSoundFontPreset(*good,0,127));
    CHECK(!hasExactSoundFontPreset(*good,1,0));
    // Exact owner-observed *shape*: 77 real presets, MIDI program 63
    // missing, 64 directly after 62. Synthetic names/sample data only.
    const auto seventySeven=makeSparse77();
    const auto sparse=inspectOriginalSoundFontPresetInventory(
        seventySeven.data);
    CHECK(sparse && sparse->size()==77u);
    CHECK((*sparse)[62].program==62u &&
          (*sparse)[62].original_phdr_row==62u);
    CHECK((*sparse)[63].program==64u &&
          (*sparse)[63].original_phdr_row==63u);
    CHECK((*sparse)[76].program==77u);
    CHECK(!hasExactSoundFontPreset(*sparse,0,63));
    CHECK(!hasExactSoundFontPreset(*sparse,0,78));
    CHECK(hasExactSoundFontPreset(*sparse,0,77));
    unsigned mutations=0;
    auto bad=f.data;
    bad[0]^=1u;
    CHECK(!inspectOriginalSoundFontPresetInventory(bad));++mutations;
    bad=f.data;bad[4]^=1u;
    CHECK(!inspectOriginalSoundFontPresetInventory(bad));++mutations;
    bad=f.data;bad[8]^=1u;
    CHECK(!inspectOriginalSoundFontPresetInventory(bad));++mutations;
    bad=f.data;bad[f.phdr+3u*38u]^=1u; // missing EOP terminal
    CHECK(!inspectOriginalSoundFontPresetInventory(bad));++mutations;
    bad=f.data;set16(bad,f.phdr+20u,128u); // invalid MIDI program
    CHECK(!inspectOriginalSoundFontPresetInventory(bad));++mutations;
    bad=f.data;set16(bad,f.phdr+38u+24u,0u); // non-monotone PBAG
    CHECK(!inspectOriginalSoundFontPresetInventory(bad));++mutations;
    bad=f.data;set16(bad,f.phdr+3u*38u+24u,7u); // terminal PBAG out of bounds
    CHECK(!inspectOriginalSoundFontPresetInventory(bad));++mutations;
    bad=f.data;
    set16(bad,f.phdr+38u+20u,0u); // duplicate same Bank/Program
    CHECK(!inspectOriginalSoundFontPresetInventory(bad));++mutations;
    bad=f.data;set16(bad,f.phdr+2u*38u+22u,16384u); // invalid SF2 bank
    CHECK(!inspectOriginalSoundFontPresetInventory(bad));++mutations;
    bad=f.data;
    // phdr chunk length is located immediately before first preset.
    set32(bad,f.phdr-4u,4u*38u-1u);
    CHECK(!inspectOriginalSoundFontPresetInventory(bad));++mutations;
    bad=f.data;
    // a physical first row cannot alias or point past the PHDR terminal.
    set16(bad,f.phdr+24u,3u);
    CHECK(!inspectOriginalSoundFontPresetInventory(bad));++mutations;
    bad=f.data;
    // Remaining bytes after last root LIST would invalidate RIFF framing.
    bad.push_back(0u);set32(bad,4u,static_cast<std::uint32_t>(bad.size()-8u));
    CHECK(!inspectOriginalSoundFontPresetInventory(bad));++mutations;
    std::cout<<"P003_SF2_LITERAL_PRESET_INVENTORY_PASS"
        <<" real_presets="<<good->size()
        <<" sparse_program=77"
        <<" separate_drum_bank=128"
        <<" mutations_rejected="<<mutations
        <<" copyright_sources=NONE\n";
    return 0;
}
