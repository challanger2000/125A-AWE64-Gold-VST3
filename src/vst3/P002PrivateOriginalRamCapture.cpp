#include "P002PrivateOriginalRamCapture.h"
#include "CreativeOriginalRamPortReplay.h"
#include "CreativeSf2RamSamplePcm.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <limits>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace awe64::vst3 {
namespace {
// All hashes were independently obtained from the same authentic
// original Win95/Creative/86Box 2026-11-13 BANK capture:
//   complete original trace: 4ec6e1764afade0ae965c2ec81cae27b...
//   filtered original SMALW/SMLD port sequence below;
//   owner SF2 bytes; actual FINAL original physical DRAM bank words.
// No Creative DSP, sample data or guest binary is embedded.
constexpr std::string_view kOwnerSf2Sha =
    "d5b99ec38fddf59f53f66b90921e170d207bf3bbd73960f6e4175c39ef68349b";
constexpr std::string_view kOriginalFilteredPortsSha =
    "fd782a9cad8a05dbe1e6e9167d8c77288fc2a24af0c7736a8501dac1afd4f84d";
constexpr std::string_view kOriginalFinalRamWordsSha =
    "ff3ce73a1898ef586312d368ad3abdc2cfad47e3cfe07dd6d460a32845d8c470";
constexpr std::size_t kSourceBankBytes=555754u;
constexpr std::size_t kFilteredOriginalPortBytes=9422232u;
constexpr std::size_t kFinalOriginalSampleWords=260921u;
constexpr std::size_t kOriginalFilteredPortEvents=523467u;
constexpr std::size_t kOriginalRamDataWrites=261264u;
constexpr std::uint32_t kSourceObservedWordBase=0x200010u;
constexpr std::size_t kOriginalMonoSampleHeaders=83u;

bool exactSha256(const std::span<const std::uint8_t> data,
                 const std::string_view expected) noexcept {
    if(data.size()>std::numeric_limits<ULONG>::max() ||
       expected.size()!=64u)return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE state=nullptr;
    std::array<UCHAR,32> hash{};
    bool success=false;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,
                                   nullptr,0)==0) {
        if(BCryptCreateHash(algorithm,&state,nullptr,0,nullptr,0,0)==0) {
            if(BCryptHashData(state,
                const_cast<PUCHAR>(
                    reinterpret_cast<const UCHAR*>(data.data())),
                static_cast<ULONG>(data.size()),0)==0 &&
               BCryptFinishHash(state,hash.data(),
                                static_cast<ULONG>(hash.size()),0)==0) {
                constexpr char digits[]="0123456789abcdef";
                success=true;
                for(std::size_t i=0;i<hash.size();++i)
                    if(expected[2u*i]!=digits[hash[i]>>4u] ||
                       expected[2u*i+1u]!=digits[hash[i]&15u])
                        success=false;
            }
            BCryptDestroyHash(state);
        }
        BCryptCloseAlgorithmProvider(algorithm,0);
    }
    return success;
}

bool exactPrivateBytes(const std::filesystem::path& filename,
                       const std::size_t expectedLength,
                       const std::string_view expectedSha,
                       std::vector<std::uint8_t>& result) {
    std::ifstream probe(filename,std::ios::binary|std::ios::ate);
    if(!probe ||
       probe.tellg()!=static_cast<std::streamoff>(expectedLength))
        return false;
    // The bank is loaded ONLY during VST3 activation; the audio
    // callback never allocates, hashes, opens or reads a file.
    std::ifstream source(filename,std::ios::binary);
    if(!source)return false;
    result.resize(expectedLength);
    if(!source.read(reinterpret_cast<char*>(result.data()),
                    static_cast<std::streamsize>(result.size())) ||
       source.peek()!=std::char_traits<char>::eof())return false;
    return exactSha256(result,expectedSha);
}

bool parseUnsigned(const std::string& text,const int base,
                   std::uint64_t& value) noexcept {
    if(text.empty())return false;
    auto [end,error]=std::from_chars(
        text.data(),text.data()+text.size(),value,base);
    return error==std::errc{} && end==text.data()+text.size();
}

bool parseExactOriginalSourcePorts(
    std::span<const std::uint8_t> raw,
    std::vector<awe64::creative::OriginalRamUploadPortWrite>& result) {
    result.clear();
    result.reserve(kOriginalFilteredPortEvents);
    const std::string originalText(
        reinterpret_cast<const char*>(raw.data()),raw.size());
    std::istringstream input(originalText);
    std::string line{};
    std::size_t physicalLines=0u;
    while(std::getline(input,line)) {
        if(++physicalLines>kOriginalFilteredPortEvents+16u)
            return false;
        if(line.empty() || line[0]=='#')continue;
        std::istringstream fields(line);
        std::string clockText,portText,valueText,extra;
        if(!(fields>>clockText>>portText>>valueText) ||
           fields>>extra)return false;
        std::uint64_t frame=0u,port=0u,value=0u;
        if(!parseUnsigned(clockText,10,frame) ||
           !parseUnsigned(portText,16,port) ||
           !parseUnsigned(valueText,16,value) ||
           frame>44100ull*4ull*3600ull ||
           port>0xffffu || value>0xffffu ||
           (port!=0xe22u && port!=0xa20u && port!=0xa22u) ||
           result.size()>=kOriginalFilteredPortEvents)
            return false;
        result.push_back({frame,static_cast<std::uint16_t>(port),
                          static_cast<std::uint16_t>(value)});
    }
    return result.size()==kOriginalFilteredPortEvents;
}
} // namespace

bool preloadOriginalCreativeCapturedRamSource(
    awe64::Chip& chip,const std::filesystem::path& privateOwnerSf2,
    const std::filesystem::path& originalFilteredCapturedRamPorts,
    const std::filesystem::path& observedOriginalFinalBankWordsPcm16,
    std::string& error) {
    error.clear();
    if(!chip.ready()) {
        error="original EMU8000 not initialized";return false;
    }
    std::vector<std::uint8_t> ownerBank,sourcePorts,observedFinalBytes;
    if(!exactPrivateBytes(privateOwnerSf2,kSourceBankBytes,
                          kOwnerSf2Sha,ownerBank)) {
        error="private owner SF2 source length/SHA256 mismatch";return false;
    }
    // Verify all 83 source sample headers using the SAME native SF2
    // reader as the instrument; this includes the seven recorded
    // two-zero-word loopEnd guards. No unsupported ROM/stereo bank.
    const auto inventory=
        awe64::creative::candidateOriginalSf2InventoryAllSampleTypes(ownerBank);
    if(!inventory ||
       inventory->sample_headers!=kOriginalMonoSampleHeaders ||
       inventory->ram_mono!=kOriginalMonoSampleHeaders ||
       inventory->rom_mono!=0u || inventory->unsupported!=0u) {
        error="original owner bank 83 RAM-mono sample inventory invalid";
        return false;
    }
    for(std::uint16_t sample=0;
        sample<kOriginalMonoSampleHeaders;++sample) {
        if(!awe64::creative::candidateOriginalSf2ReadRamMonoSamplePcm(
               ownerBank,sample,awe64::kSampleRamBytes/2u)) {
            error="original owner bank RAM SF2 sample/loop validation failed";
            return false;
        }
    }
    if(!exactPrivateBytes(originalFilteredCapturedRamPorts,
                          kFilteredOriginalPortBytes,
                          kOriginalFilteredPortsSha,sourcePorts)) {
        error="original 86Box/Win95 captured RAM ports SHA256 mismatch";
        return false;
    }
    if(!exactPrivateBytes(observedOriginalFinalBankWordsPcm16,
                          kFinalOriginalSampleWords*2u,
                          kOriginalFinalRamWordsSha,observedFinalBytes)) {
        error="actual ORIGINAL Creative final RAM words SHA256 mismatch";
        return false;
    }
    std::vector<std::uint16_t> expectedFinal(kFinalOriginalSampleWords);
    for(std::size_t i=0;i<expectedFinal.size();++i)
        expectedFinal[i]=static_cast<std::uint16_t>(
            static_cast<unsigned>(observedFinalBytes[2u*i])|
            (static_cast<unsigned>(observedFinalBytes[2u*i+1u])<<8u));
    std::vector<awe64::creative::OriginalRamUploadPortWrite> ports;
    if(!parseExactOriginalSourcePorts(sourcePorts,ports)) {
        error="original filtered RAM port syntax/length invalid";
        return false;
    }
    // NO guessed allocator/transform/loop. This is the EXACT 1996
    // Creative driver port capture. Accept legitimate rewrites only
    // when the FINAL ram matches source-observed words on all 260921.
    // The independently SHA-pinned source assets serve as the genuine
    // provenance for this ONE bank, not arbitrary external data.
    const auto verified=
        awe64::creative::candidateReplayObservedOriginalCreativeFinalRamBank(
            chip,ports,kSourceObservedWordBase,expectedFinal,true,true);
    if(!verified ||
       verified->original_port_upload.exact_port_writes!=ports.size() ||
       verified->original_port_upload.original_sample_words!=
           kOriginalRamDataWrites ||
       verified->independently_verified_final_word_count!=
           kFinalOriginalSampleWords ||
       verified->source_sample_distinct_rewritten_words!=255u) {
        error="source original chip RAM replay/final-state gate refused";
        return false;
    }
    // For this test-only mode, directly inspect the original chip RAM
    // AFTER replay. This is a research/contract check, not a new DSP
    // or an alternative production sampler/allocator.
    const auto* original=chip.rawForContractTesting();
    if(!original || !original->ram) {
        error="original EMU8000 physical RAM inaccessible";return false;
    }
    const auto offset=kSourceObservedWordBase-awe64::kSampleRamBase;
    for(std::size_t i=0;i<expectedFinal.size();++i) {
        if(static_cast<std::uint16_t>(original->ram[offset+i])!=
           expectedFinal[i]) {
            error="original chip DRAM final bank word readback differs";
            return false;
        }
    }
    return true;
}

// Four byte-for-byte authenticated ORIGINAL Creative 1996 note command
// groups, captured from the same private Win95/Fork source whose full
// original register trace was SHA256 50fe55de...433cac05f9c.
// Their source original frames, not a post-hoc guessed ADSR/velocity,
// define the real relative hardware-clock port-write schedule.
void P002PrivateOriginalRamVoices::clear() noexcept {
    ready_=false;held_.fill(false);pending_group_=2u;
    cursor_=0u;frame_=0u;
    c4_on_count_=c4_off_count_=d4_on_count_=d4_off_count_=0u;
    c4_on_.fill({});c4_off_.fill({});d4_on_.fill({});d4_off_.fill({});
}
bool P002PrivateOriginalRamVoices::load(
    const std::array<std::filesystem::path,4>& paths,
    std::string& error) {
    clear();
    constexpr std::array<std::size_t,4> sizes{
        2584u,136u,2652u,136u};
    constexpr std::array<std::size_t,4> eventCounts{
        152u,8u,156u,8u};
    constexpr std::array<std::uint32_t,4> firstFrames{
        3154566u,3172293u,3201361u,3219527u};
    constexpr std::array<std::uint32_t,4> lastFrames{
        3154568u,3172293u,3201363u,3219527u};
    constexpr std::array<std::string_view,4> digests{
        "ad73939fdbcb19c5a47877098ac739d0207e96261ee947ade85f7a0514a6fd31",
        "c49018ad0edd3bd3df58b19dc401e75d8e41a6b6ae15bf2c135fc298b07afe62",
        "db07000b0c3cd8903a32969863b4507428a09ef923bc6a9b00b1794f19c991f2",
        "a61cd7a02a553d62e06eeeb97fdc4b57bb2d73e26f11d633de9de52ddebfd514"};
    auto fail=[&](const char* why) {
        error=why;clear();return false;
    };
    auto read=[&](const std::size_t group,
                  const std::span<SourcePort> destination)->bool {
        std::vector<std::uint8_t> bytes;
        if(!exactPrivateBytes(paths[group],sizes[group],digests[group],bytes))
            return false;
        std::istringstream reader(std::string(
            reinterpret_cast<const char*>(bytes.data()),bytes.size()));
        std::string line;
        std::size_t n=0u;
        std::uint64_t prior=0u;
        while(std::getline(reader,line)) {
            std::istringstream items(line);
            std::string f,p,v,extra;
            if(!(items>>f>>p>>v) || items>>extra ||
               n>=eventCounts[group] || n>=destination.size())
                return false;
            std::uint64_t frame=0u,port=0u,value=0u;
            if(!parseUnsigned(f,10,frame) ||
               !parseUnsigned(p,16,port) ||
               !parseUnsigned(v,16,value) ||
               frame<firstFrames[group] || frame>lastFrames[group] ||
               (n>0u && frame<prior) || port>65535u ||
               value>65535u ||
               (port!=0xe22u && port!=0xe20u && port!=0xa20u &&
                port!=0xa22u && port!=0x620u && port!=0x622u))
                return false;
            prior=frame;
            destination[n++] = SourcePort{
                static_cast<std::uint32_t>(frame-firstFrames[group]),
                static_cast<std::uint16_t>(port),
                static_cast<std::uint16_t>(value)};
        }
        return n==eventCounts[group] &&
            destination.front().relative_frame==0u &&
            destination[n-1u].relative_frame==
                lastFrames[group]-firstFrames[group];
    };
    if(!read(0u,c4_on_) || !read(1u,c4_off_) ||
       !read(2u,d4_on_) || !read(3u,d4_off_))
        return fail("original Win95 RAM C4/D4 register source SHA/timing invalid");
    c4_on_count_=static_cast<std::uint16_t>(eventCounts[0]);
    c4_off_count_=static_cast<std::uint16_t>(eventCounts[1]);
    d4_on_count_=static_cast<std::uint16_t>(eventCounts[2]);
    d4_off_count_=static_cast<std::uint16_t>(eventCounts[3]);
    ready_=true;
    error.clear();
    return true;
}
bool P002PrivateOriginalRamVoices::noteOn(
    const std::uint8_t channel,const std::uint8_t key,
    const std::uint8_t velocity) noexcept {
    if(!ready_ || channel!=0u || velocity!=100u || pending() ||
       (key!=60u && key!=62u))return false;
    const auto group=static_cast<std::uint8_t>(key==60u?0u:1u);
    if(held_[group])return false;
    held_[group]=true;
    pending_group_=group;cursor_=0u;frame_=0u;
    return true;
}
void P002PrivateOriginalRamVoices::advanceOneChipFrame(
    awe64::Chip& chip) noexcept {
    if(!ready_ || !pending() || !chip.ready())return;
    const bool c4=pending_group_==0u;
    const auto* source=c4?c4_on_.data():d4_on_.data();
    const auto count=c4?c4_on_count_:d4_on_count_;
    while(cursor_<count && source[cursor_].relative_frame==frame_) {
        chip.writePort(source[cursor_].port,source[cursor_].value);
        ++cursor_;
    }
    ++frame_;
    if(cursor_>=count)pending_group_=2u;
}
bool P002PrivateOriginalRamVoices::noteOff(
    awe64::Chip& chip,const std::uint8_t channel,
    const std::uint8_t key) noexcept {
    if(!ready_ || !chip.ready() || channel!=0u || pending() ||
       (key!=60u && key!=62u))return false;
    const auto group=static_cast<std::uint8_t>(key==60u?0u:1u);
    if(!held_[group])return false;
    const auto* source=group==0u?c4_off_.data():d4_off_.data();
    const auto count=group==0u?c4_off_count_:d4_off_count_;
    for(std::size_t i=0;i<count;++i)
        chip.writePort(source[i].port,source[i].value);
    held_[group]=false;
    return true;
}
} // namespace awe64::vst3
