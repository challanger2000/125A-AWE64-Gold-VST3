// P002 PRIVATE research executable: original Win95/SBAWE.VXD
// RAM upload port replay matched against a REAL external SF2 sample.
// This utility runs OFFLINE, outside the DAW/audio callback. It does
// NOT produce Native Creative allocator logic, convert RAM SF2 banks
// to live VST3 instruments, or establish original Win95 PCM parity.
//
// First independently verify original ROM SHA256 with
//   python tools/check_private_rom.py --rom ORIGINAL.raw --sha256 4e143...
// Then independently verify Win95 driver/trace provenance. Generate
// a FILTERED immutable PRIVATE port trace with:
//   tools/p002_original_ram_trace_probe.py --original-emu8k-trace ...
//    --private-sf2 ... --sample-id ... --private-report ...
//    --private-filtered-ram-replay ...
// NEVER upload original copyrighted source/sample bytes to GitHub.
//
// CLI:
//   awe64_p002_private_ram_capture_cli ORIGINAL.raw bank.sf2
//     sample_id source_observed_emu8000_word_address
//     filtered_original_ram_ports.trace
//     --original-guest-source-independently-reviewed
//
// The final assertion is caller supplied: the executable cannot
// independently establish the historical source of the recording.
#include "CreativeOriginalRamPortReplay.h"
#include "CreativeSf2RamSamplePcm.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#endif
#include <86box/snd_emu8k.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using awe64::creative::OriginalRamUploadPortWrite;
namespace fs=std::filesystem;
void verifyPinnedOriginalCreativeRom(const fs::path& path) {
    // The private offline program must not silently accept a same-
    // length but WRONG Creative ROM. The VST3 CI uses this exact
    // independently pinned SHA256. No ROM bytes leave the host.
    constexpr std::array<std::uint8_t,32> sourceSha{{
        0x4e,0x14,0x3b,0x94,0xf7,0x58,0x73,0x4f,
        0x59,0x4d,0xed,0x78,0xf4,0xe5,0x11,0x56,
        0x35,0x97,0x5c,0x16,0xfa,0x37,0xfa,0xbe,
        0xae,0x0b,0xaa,0x49,0x38,0xca,0x71,0x0e}};
#ifdef _WIN32
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file || file.tellg()!=1048576)
        throw std::runtime_error("Original AWE32 ROM must be exact 1 MiB");
    file.seekg(0);
    std::vector<std::uint8_t> bytes(1048576u);
    if(!file.read(reinterpret_cast<char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Original ROM identity read failed");
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    auto status=BCryptOpenAlgorithmProvider(
        &algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0u);
    if(status<0 || algorithm==nullptr)
        throw std::runtime_error("Windows CNG SHA256 unavailable");
    std::array<std::uint8_t,32> found{};
    status=BCryptHash(algorithm,nullptr,0u,bytes.data(),
                      static_cast<ULONG>(bytes.size()),found.data(),
                      static_cast<ULONG>(found.size()));
    BCryptCloseAlgorithmProvider(algorithm,0u);
    if(status<0 || found!=sourceSha)
        throw std::runtime_error(
            "Private original Creative ROM SHA256 mismatch");
#else
    (void)path;(void)sourceSha;
    throw std::runtime_error(
        "Private ROM SHA256 gate currently requires Windows CNG");
#endif
}
std::vector<std::uint8_t> readSf2(const fs::path& p) {
    std::ifstream f(p,std::ios::binary|std::ios::ate);
    if(!f || f.tellg()<12 ||
       f.tellg()>static_cast<std::streamoff>(32u*1024u*1024u))
        throw std::runtime_error("Private standard RAM SoundFont missing or oversized");
    const auto n=static_cast<std::size_t>(f.tellg());
    std::vector<std::uint8_t> bytes(n);
    f.seekg(0);
    if(!f.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Private SF2 read truncated");
    return bytes;
}
std::uint64_t parseUnsigned(const std::string& s,const int base) {
    std::size_t consumed=0;
    const auto val=std::stoull(s,&consumed,base);
    if(consumed!=s.size())
        throw std::runtime_error("Extra trailing number characters");
    return val;
}
std::vector<OriginalRamUploadPortWrite>
readFilteredOriginalCapture(const fs::path& p) {
    std::ifstream f(p);
    if(!f || !fs::is_regular_file(p) ||
       fs::file_size(p)>1'200'000'000ull)
        throw std::runtime_error("Private original source capture absent/oversize");
    std::vector<OriginalRamUploadPortWrite> trace{};
    trace.reserve(4096u);
    std::string line{};
    std::uint64_t prev=0u;
    bool first=true;
    while(std::getline(f,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::istringstream fields(line);
        std::string frame{},port{},value{},extra{};
        if(!(fields>>frame>>port>>value) || (fields>>extra))
            throw std::runtime_error("Original port capture record malformed");
        const auto tick=parseUnsigned(frame,10);
        const auto io=parseUnsigned(port,16);
        const auto word=parseUnsigned(value,16);
        if(tick>44'100ull*4ull*3600ull ||
           io>65535ull || word>65535ull ||
           (!first && tick<prev))
            throw std::runtime_error("Original port capture clock/16-bit value invalid");
        first=false;prev=tick;
        if(trace.size()>=2u*awe64::kSampleRamBytes+1024u)
            throw std::runtime_error("Original upload record count exceeds RAM budget");
        trace.push_back({tick,static_cast<std::uint16_t>(io),
                             static_cast<std::uint16_t>(word)});
    }
    if(!f.eof() || trace.empty())
        throw std::runtime_error("Original captured RAM port trace incomplete");
    return trace;
}
int run(int argc,char** argv) {
    if(argc!=7 ||
       std::string(argv[6])!="--original-guest-source-independently-reviewed") {
        std::cerr<<"USAGE: awe64_p002_private_ram_capture_cli "
                   "ORIGINAL_ROM.raw private.sf2 sample_id "
                   "observed_word_address private_filtered_ram.trace "
                   "--original-guest-source-independently-reviewed\n"
                   "Original Win95/SBAWE.VXD provenance and ROM SHA256 "
                   "must be checked INDEPENDENTLY before this source audit.\n";
        return 2;
    }
    const fs::path rom(argv[1]),sf2(argv[2]),tracefile(argv[5]);
    const auto sampleId=parseUnsigned(argv[3],10);
    const auto base=parseUnsigned(argv[4],0);
    if(sampleId>65535ull || base>std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("Unbounded private sample ID or source RAM address");
    const auto bank=readSf2(sf2);
    const auto sample=awe64::creative::candidateOriginalSf2ReadRamMonoSamplePcm(
        bank,static_cast<std::uint16_t>(sampleId),
        awe64::kSampleRamBytes/2u);
    if(!sample)
        throw std::runtime_error("No validated mono 16-bit RAM SF2 sample");
    const auto trace=readFilteredOriginalCapture(tracefile);
    verifyPinnedOriginalCreativeRom(rom);
    awe64::Chip chip{};
    std::string error{};
    if(!chip.open(rom,error))
        throw std::runtime_error("Pinned original chip/ROM init failed: "+error);
    const auto matched=
        awe64::creative::candidateReplaySourceMatchedOriginalRamSf2Sample(
            chip,bank,static_cast<std::uint16_t>(sampleId),
            static_cast<std::uint32_t>(base),trace,true);
    if(!matched)
        throw std::runtime_error(
            "SOURCE RAM SAMPLE vs ORIGINAL GUEST SMALW/SMLD FAIL: "
            "no chip RAM upload accepted");
    const auto* original=chip.rawForContractTesting();
    if(!original || !original->ram ||
       base<awe64::kSampleRamBase)
        throw std::runtime_error("Original chip DRAM verification unavailable");
    const auto start=static_cast<std::size_t>(base-awe64::kSampleRamBase);
    const auto count=sample->sample_pcm_le16.size()/2u;
    if(start+count>awe64::kSampleRamBytes/2u)
        throw std::runtime_error("Post-upload source RAM word bounds invalid");
    for(std::size_t i=0u;i<count;++i) {
        const auto pcm=sample->sample_pcm_le16;
        const auto word=static_cast<std::uint16_t>(
            static_cast<unsigned>(pcm[2u*i])|
            (static_cast<unsigned>(pcm[2u*i+1u])<<8u));
        if(static_cast<std::uint16_t>(original->ram[start+i])!=word)
            throw std::runtime_error(
                "Pinned original chip RAM readback differs from SF2 PCM");
    }
    std::cout<<"P002_PRIVATE_SOURCE_MATCHED_RAM_SF2_CHIP_UPLOAD_PASS"
             <<" copied_original_port_events="<<matched->exact_port_writes
             <<" original_ram_word_writes="<<matched->original_sample_words
             <<" sample_checked_words="<<count
             <<" source_capture_word_base=0x"<<std::hex<<base<<std::dec
             <<" original_chip_RAM_sample_readback_exact=TRUE"
             <<" pinned_original_AWE32_ROM_SHA256_verified=TRUE"
             <<" source_guest_provenance=CALLER_ATTESTED_NOT_INDEPENDENTLY_PROVEN_BY_CLI"
             <<" original_driver_program_software=NOT_IMPLEMENTED"
             <<" native_vst3_RAM_audio_and_Win95_pcm_parity=NOT_PROVEN\n";
    return 0;
}
} // namespace
int main(int argc,char** argv) {
    try {return run(argc,argv);}
    catch(const std::exception& e) {
        std::cerr<<"P002_PRIVATE_RAM_SOURCE_AUDIT_NO_GO "<<e.what()<<'\n';
        return 1;
    }
}
