// Windows-x64 private research test: same native PCOD/149? NO:
// exact 148-byte original Creative driver descriptor builder as VST3.
// Reads only user-owned source SHA-verified bank and VXD tables supplied
// separately. No original ROM, VXD, SF2 or PCM is shipped with this tool.
#include "CreativeSf2RamSamplePcm.h"
#include "CreativeSf2VoiceDescriptor.h"
#include "CreativeSbkLayers.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <string_view>
#include <algorithm>
#pragma comment(lib,"bcrypt.lib")
using namespace awe64::creative;
namespace {
constexpr std::string_view kOwnerBankSha=
    "d5b99ec38fddf59f53f66b90921e170d207bf3bbd73960f6e4175c39ef68349b";
constexpr std::string_view kSourceVxdTableSha=
    "0703643c6df3a8af8aa3ee94a272b40ff349462245f6e2491c27dc3344a1761a";
bool shaMatches(const std::vector<std::uint8_t>& b,
                const std::string_view expected) noexcept {
    if(expected.size()!=64u || b.empty() || b.size()>8000000u)
        return false;
    BCRYPT_ALG_HANDLE alg=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    std::array<std::uint8_t,32> digest{};
    bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,
                                       nullptr,0u)==0;
    if(ok)ok=BCryptCreateHash(alg,&hash,nullptr,0u,nullptr,0u,0u)==0;
    if(ok)ok=BCryptHashData(hash,const_cast<PUCHAR>(b.data()),
                           static_cast<ULONG>(b.size()),0u)==0;
    if(ok)ok=BCryptFinishHash(hash,digest.data(),
                             static_cast<ULONG>(digest.size()),0u)==0;
    if(hash)BCryptDestroyHash(hash);
    if(alg)BCryptCloseAlgorithmProvider(alg,0u);
    if(!ok)return false;
    constexpr std::string_view hex="0123456789abcdef";
    for(std::size_t i=0u;i<digest.size();++i)
        if(expected[i*2u]!=hex[digest[i]>>4u] ||
           expected[i*2u+1u]!=hex[digest[i]&15u])return false;
    return true;
}
bool readExact(const std::filesystem::path& path,
               const std::size_t length,
               const std::string_view sha,
               std::vector<std::uint8_t>& target) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in || in.tellg()!=static_cast<std::streamoff>(length))
        return false;
    in.seekg(0);
    target.resize(length);
    return bool(in.read(reinterpret_cast<char*>(target.data()),
                        static_cast<std::streamsize>(length))) &&
           shaMatches(target,sha);
}
unsigned word32(const auto& bytes,const unsigned offset) noexcept {
    return unsigned(bytes[offset])|
           (unsigned(bytes[offset+1u])<<8u)|
           (unsigned(bytes[offset+2u])<<16u)|
           (unsigned(bytes[offset+3u])<<24u);
}
int fail(const char* message) {
    std::cerr<<"P002_REAL_OWNER_RAM_SF2_DESCRIPTOR_NO_GO: "<<message<<"\n";
    return 2;
}
} // namespace

int wmain(int argc,wchar_t** argv) {
    if(argc!=3)
        return fail("two PRIVATE input paths: source SF2 and original VXD 820-byte table pack");
    std::vector<std::uint8_t> sf2,vxd;
    if(!readExact(std::filesystem::path(argv[1]),555754u,kOwnerBankSha,sf2))
        return fail("not exact SHA-locked original owner Synth 512 Bank.sf2");
    if(!readExact(std::filesystem::path(argv[2]),820u,kSourceVxdTableSha,vxd))
        return fail("not exact SHA-locked original 1996 SBAWE.VXD LCOD/PDAT tables");
    OriginalCreativeSf2DriverResources source{};
    std::size_t at=0u;
    const auto le16=[&](const std::size_t i) noexcept {
        return static_cast<std::int16_t>(
            static_cast<std::uint16_t>(
                unsigned(vxd[i]) | (unsigned(vxd[i+1u])<<8u)));
    };
    const auto le32=[&](const std::size_t i) noexcept {
        return static_cast<std::uint32_t>(unsigned(vxd[i])|
                (unsigned(vxd[i+1u])<<8u)|
                (unsigned(vxd[i+2u])<<16u)|
                (unsigned(vxd[i+3u])<<24u));
    };
    for(auto& w:source.instrument_template) {
        w=le16(at);at+=2u;
    }
    for(auto& w:source.pitch_tables.base) {
        w=le32(at);at+=4u;
    }
    for(auto& b:source.pitch_tables.slope)
        b=static_cast<std::int8_t>(vxd[at++]);
    if(at!=758u)return fail("original VXD table layout invalid");
    constexpr std::uint32_t originalSampleRamBase=0x200010u;
    constexpr std::size_t originalSampleWords=260921u;
    unsigned realLayerCount=0u;
    for(const auto key:{std::uint8_t{60},std::uint8_t{62}}) {
        const auto zones=candidateStandardSf2RamLayers(sf2,0u,0u,key,100u);
        const auto samples=candidateOriginalSf2RamMonoNotePcmPlan(
            sf2,0u,0u,key,100u,0xe00000u);
        const auto physical=candidateOriginalSf2RamPhysicalNotePlan(
            sf2,0u,0u,key,100u,
            originalSampleRamBase,originalSampleWords,true);
        if(!zones || !samples || !physical ||
           zones->count!=2u || samples->count!=2u ||
           physical->count!=2u)
            return fail("authentic source 2-layer SF2 mapping mismatch");
        OriginalSf2VoiceStartConditions context{
            0u,key,100u,64u,0u,0u,0u,{}};
        for(unsigned i=0;i<2u;++i) {
            if(samples->layers[i].sample_id!=82u ||
               physical->layers[i].sample_id!=82u)
                return fail("real SF2 source sample 82 not selected");
            const auto descriptor=candidateOriginalSf2RamNoteFromVerifiedVxdResources(
                zones->layers[i],samples->layers[i],physical->layers[i],
                source,zones->layers[i].original_preset_row_index,
                context,true);
            if(!descriptor ||
               word32(descriptor->before_note_start,0x76u)!=0x23F8CFu ||
               word32(descriptor->before_note_start,0x7eu)!=0x23F8D8u ||
               word32(descriptor->before_note_start,0x82u)!=0x23FA69u)
                return fail("original VXD PCOD 148-byte sample address mismatch");
            const auto ip=candidateOriginalSf2InitialIpFromDescriptor(
                descriptor->after_note_start,key,0,0);
            if(!ip)return fail("original VXD PCOD 148-byte IP pitch invalid");
            std::cout<<"P002_OWNER_REAL_VXD_SF2_PC0D_NOTE_DESCRIPTOR_PASS"
                     <<" key="<<unsigned(key)
                     <<" layer="<<i
                     <<" sample_id=82"
                     <<" selected_program=0"
                     <<" pitch_ip=0x"<<std::hex<<*ip<<std::dec
                     <<" final_native_driver_PCM_parity=NOT_PROVEN\n";
            ++realLayerCount;
        }
    }
    return realLayerCount==4u?0:fail("four original C4/D4 layers not reproduced");
}
