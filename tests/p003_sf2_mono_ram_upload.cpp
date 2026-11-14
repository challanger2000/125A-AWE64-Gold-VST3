// P003: actual original EMU8000 sample-RAM transfer from standard SF2.
// Synthetic 1-MiB ROM and synthetic SoundFont only; no Creative media.
// Source byte order and every physical RAM word are independently checked.
#include "CreativeSf2RamBankUpload.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>
using Bytes=std::vector<std::uint8_t>;
using namespace awe64::creative;
#define CHECK(p) do {if(!(p)){std::cerr<<"P003_REAL_MONO_RAM_UPLOAD_FAIL at "<<__LINE__<<" "<<#p<<"\n";return 1;}}while(false)
namespace {
void w16(Bytes& out,unsigned v){out.push_back(v&255u);out.push_back((v>>8u)&255u);}
void w32(Bytes& out,std::uint32_t v){w16(out,v&65535u);w16(out,v>>16u);}
void chunk(Bytes& out,std::string_view name,const Bytes& data) {
    out.insert(out.end(),name.begin(),name.end());
    w32(out,static_cast<std::uint32_t>(data.size()));
    out.insert(out.end(),data.begin(),data.end());
    if(data.size()%2u)out.push_back(0u);
}
void name(Bytes& out,std::string_view text) {
    out.insert(out.end(),text.begin(),text.end());
    out.insert(out.end(),20u-text.size(),0);
}
void sample(Bytes& out,std::string_view label,
            std::uint32_t first,std::uint32_t last,
            std::uint32_t loopFirst,std::uint32_t loopLast,
            std::uint16_t type=1u) {
    name(out,label);w32(out,first);w32(out,last);
    w32(out,loopFirst);w32(out,loopLast);w32(out,44100u);
    out.push_back(60u);out.push_back(0u);
    w16(out,0u);w16(out,type);
}
Bytes sf2(bool nonMono=false,bool withSm24=false,
          bool invalidLoop=false,bool duplicatedSmpl=false) {
    Bytes info{'I','N','F','O'};
    chunk(info,"ifil",{2u,0u,1u,0u});
    Bytes smpl;
    for(const auto w:{0xffffu,0x8000u,0x0000u,0x7fffu,
                      0x1234u,0x8001u,0xa55au,0x0000u})
        w16(smpl,w);
    Bytes sdta{'s','d','t','a'};
    chunk(sdta,"smpl",smpl);
    if(withSm24)chunk(sdta,"sm24",{1u,2u,3u,4u});
    if(duplicatedSmpl)chunk(sdta,"smpl",smpl);
    Bytes shdr;
    sample(shdr,"MONO-A",0,4,1,3);
    sample(shdr,"MONO-B",4,8,5,invalidLoop?5:7,
           nonMono?0x0004u:0x0001u);
    sample(shdr,"EOS",0,0,0,0,0u);
    Bytes pdta{'p','d','t','a'};
    chunk(pdta,"shdr",shdr);
    Bytes root{'s','f','b','k'};
    chunk(root,"LIST",info);
    chunk(root,"LIST",sdta);
    chunk(root,"LIST",pdta);
    Bytes riff{'R','I','F','F'};
    w32(riff,static_cast<std::uint32_t>(root.size()));
    riff.insert(riff.end(),root.begin(),root.end());
    return riff;
}
}
int main() {
    const auto rom=std::filesystem::temp_directory_path()/
        "125a-p003-real-emu8000-synthetic-rom-mono-preload.raw";
    {
        std::ofstream file(rom,std::ios::binary|std::ios::trunc);
        CHECK(file.good());
        const std::array<char,4096> zeros{};
        for(unsigned i=0;i<256u;++i)file.write(zeros.data(),zeros.size());
        CHECK(file.good());
    }
    awe64::Chip chip;
    std::string error;
    CHECK(chip.open(rom,error));
    std::error_code ec;std::filesystem::remove(rom,ec);
    auto* original=chip.rawForContractTesting();
    CHECK(original && original->ram);
    const Bytes bank=sf2();
    const auto whole=candidateStandardSf2WholeMonoRamPool(bank);
    CHECK(whole && whole->sample_headers==2u);
    CHECK(whole->whole_smpl_pcm_le16.size()==16u);
    P003RamUploadResult outcome{};
    constexpr auto first=std::uint32_t(awe64::kSampleRamBase+16u);
    CHECK(p003UploadValidatedMonoSf2PoolToOriginalChip(
        chip,bank,first,outcome,error));
    CHECK(error.empty() && outcome.entire_pool_readback_equal);
    CHECK(outcome.source_pool_words==8u &&
          outcome.sample_headers==2u &&
          outcome.physical_word_base==first);
    const std::array<std::uint16_t,8> expected{
        0xffffu,0x8000u,0x0000u,0x7fffu,
        0x1234u,0x8001u,0xa55au,0x0000u};
    const auto at=first-awe64::kSampleRamBase;
    for(std::size_t i=0;i<8u;++i)
        CHECK(static_cast<std::uint16_t>(original->ram[at+i])==expected[i]);
    // A new successful load at exactly the last eight permitted words
    // may not alter the thirty-two reserved FM words 0xffffe0..ffffff.
    constexpr std::uint32_t lastAllowed=awe64::kFmReservedBase-8u;
    const auto reservedBefore=original->ram[
        awe64::kFmReservedBase-awe64::kSampleRamBase];
    CHECK(p003UploadValidatedMonoSf2PoolToOriginalChip(
        chip,bank,lastAllowed,outcome,error));
    CHECK(outcome.source_pool_words==8u &&
          outcome.physical_word_base==lastAllowed);
    for(std::size_t i=0;i<8u;++i)
        CHECK(static_cast<std::uint16_t>(
            original->ram[lastAllowed-awe64::kSampleRamBase+i])==expected[i]);
    CHECK(original->ram[awe64::kFmReservedBase-awe64::kSampleRamBase]==
          reservedBefore);
    const auto before=original->ram[at];
    auto reject=[&](std::span<const std::uint8_t> broken,
                    std::uint32_t physicalBase) {
        P003RamUploadResult bad{123u,7u,1u,true};
        error.clear();
        const bool accepted=p003UploadValidatedMonoSf2PoolToOriginalChip(
            chip,broken,physicalBase,bad,error);
        return !accepted && !error.empty() &&
               !bad.entire_pool_readback_equal &&
               original->ram[at]==before;
    };
    unsigned rejected=0u;
    CHECK(reject(sf2(true),first));++rejected;
    CHECK(reject(sf2(false,true),first));++rejected;
    CHECK(reject(sf2(false,false,true),first));++rejected;
    CHECK(reject(sf2(false,false,false,true),first));++rejected;
    auto corrupt=bank;corrupt[4u]^=1u;
    CHECK(reject(corrupt,first));++rejected;
    CHECK(reject(bank,awe64::kSampleRamBase+3u));++rejected;
    CHECK(reject(bank,awe64::kFmReservedBase-7u));++rejected;
    CHECK(reject(bank,awe64::kFmReservedBase));++rejected;
    // A bank must not be partially written after any bad sample header,
    // absent smpl data, invalid base or reserved memory overlap.
    chip.close();
    P003RamUploadResult afterClose{};
    error.clear();
    CHECK(!p003UploadValidatedMonoSf2PoolToOriginalChip(
        chip,bank,first,afterClose,error));
    CHECK(!error.empty() && !afterClose.entire_pool_readback_equal);
    ++rejected;
    std::cout<<"P003_REAL_EMU8000_SF2_MONO_RAM_TRANSFER_PASS "
        <<"physical_words=8 full_readback=TRUE "
        <<"boundary_last_word=0xffffdf"
        <<" negative_controls="<<rejected
        <<" original_creative_allocator=NOT_CLAIMED"
        <<" external_proprietary_assets=NONE\n";
    return 0;
}
