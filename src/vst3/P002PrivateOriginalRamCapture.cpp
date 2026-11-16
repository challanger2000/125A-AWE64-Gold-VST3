#include "P002PrivateOriginalRamCapture.h"
#include "CreativeOriginalRamPortReplay.h"
#include "CreativeSf2RamSamplePcm.h"
#include "CreativeSf2RamBankUpload.h"
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
// Multiple source-proven genuine Win95 Creative sessions, no private bytes.
struct ExactSourceRamCapture {
    std::size_t bytes;
    std::string_view sha256;
    std::size_t port_events;
    std::size_t original_smlD_word_writes;
};
constexpr std::array<ExactSourceRamCapture,4> kOriginalCaptures{{
    // Historical original session recorded and reviewed on 2026-11-13.
    {9422232u,
     "fd782a9cad8a05dbe1e6e9167d8c77288fc2a24af0c7736a8501dac1afd4f84d",
     523467u,261264u},
    // Repeated ACTUAL original AWEMAN32.DLL/SBAWE.VXD boot 2026-11-14.
    // Original full TRACE SHA256:
    // 53f6de54e1d6a487cd7bbb01aeba3538409019a7cebf55afa3cbc02863503ece
    // Real C:\P002RAM.LOG SHA256:
    // a29a6b68a17c119d1d4683de2930d842ed75f7520d773e79ff458c7c47b0645a
    // Actual 44.1kHz pre-card AWE32_WAV SHA256:
    // 5408731eb4c2781149e4a3904c7f219d4dd7534575f121dea83c3cb514f7af88
    // Source original RAM upload begins at actual chip frame 4436947.
    {8894094u,
     "cbde9d3a7cd40885dbb19008b4fa1551c0a6e74bfa3080317a98fcdfc36101b1",
     523182u,261207u},
    // Independently SHA-attested calibrated Win95 CC0=1 owner reboot,
    // session 2026-11-14T18:09:37Z. Source bytes stay private.
    {9422232u,
     "6674b2034dfa59c6ff6e69bdb4f4cdfbb9202294fc7f8ac5e1a3dd0f8d183805",
     523467u,261264u},
    // Independently captured ORIGINAL 2026-11-15 Win95/Creative user
    // SoundFont 1 RAM upload from the user's successful 2-bank run.
    // Full source trace SHA256:
    // 181b2bc29a0a3118ea6eb8bc6d921b5447b2c9bf5714356b85d1176e4a6e9548
    // Guest log SHA256:
    // 10a50fdb5b5faaff84b11e6c830e2399532a493ce1d41e6807fdd2787c178441
    // Pre-card original WAV SHA256:
    // 35315175420592f5424bbcd9f7d0d73a151fb49989f7b8ec69aa5138c99a8e12
    // Observed original first-bank SMALW start: frame 1603578.
    // All 260921 original words, 255 overwritten addresses and
    // final RAM SHA match independent historical original capture.
    {8894094u,
     "f4d833a01173e6879a990669f430bc7e91d3df07ba1ee2b91c6bcf8af03d993d",
     523182u,261207u},
}};
constexpr std::string_view kOriginalFinalRamWordsSha =
    "ff3ce73a1898ef586312d368ad3abdc2cfad47e3cfe07dd6d460a32845d8c470";
constexpr std::size_t kSourceBankBytes=555754u;
constexpr std::size_t kFinalOriginalSampleWords=260921u;
constexpr std::size_t kMaxFilteredOriginalPortEvents=523467u;
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
    result.reserve(kMaxFilteredOriginalPortEvents);
    const std::string originalText(
        reinterpret_cast<const char*>(raw.data()),raw.size());
    std::istringstream input(originalText);
    std::string line{};
    std::size_t physicalLines=0u;
    while(std::getline(input,line)) {
        if(++physicalLines>kMaxFilteredOriginalPortEvents+16u)
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
           result.size()>=kMaxFilteredOriginalPortEvents)
            return false;
        result.push_back({frame,static_cast<std::uint16_t>(port),
                          static_cast<std::uint16_t>(value)});
    }
    return !result.empty();
}
} // namespace

bool isExactOriginalCreativeVxdResources(
    const std::span<const std::uint8_t> five_original_tables) noexcept {
    // Independently extracted from the owner's exact original
    // SBAWE.VXD SHA256 d343f242... (original LE page-mapped objects).
    // Source bytes: LCOD+0x2D88 [59 signed WORDs], PDAT+0x08E8 [128
    // DWORDs], PDAT+0x0AE8 [128 signed BYTEs], LCOD+0x2F82 [15
    // WORDs], LCOD+0x2FA0 [16 WORDs]. The public code retains ONLY
    // the SHA256 digest, not copyrighted Creative table contents.
    constexpr std::size_t exactOriginalTableBytes=
        59u*2u+128u*4u+128u+15u*2u+16u*2u; // 820
    constexpr std::string_view exactOriginalVxdTableSha256=
        "0703643c6df3a8af8aa3ee94a272b40ff349462245f6e2491c27dc3344a1761a";
    return five_original_tables.size()==exactOriginalTableBytes &&
           exactSha256(five_original_tables,exactOriginalVxdTableSha256);
}

bool isExactOriginalCreativeRamOwnerSf2(
    const std::span<const std::uint8_t> source) noexcept {
    return source.size()==kSourceBankBytes &&
           exactSha256(source,kOwnerSf2Sha);
}

bool preloadOriginalCreativeCapturedRamSource(
    awe64::Chip& chip,const std::filesystem::path& privateOwnerSf2,
    const std::filesystem::path& originalFilteredCapturedRamPorts,
    const std::filesystem::path& observedOriginalFinalBankWordsPcm16,
    std::string& error,
    awe64::creative::P003RamUploadResult* verified_capture) {
    error.clear();
    if(verified_capture)*verified_capture={};
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
    const ExactSourceRamCapture* realSession=nullptr;
    for(const auto& candidate:kOriginalCaptures) {
        // STRICT original physical source binding: both exact byte count
        // and immutable digest are required, NEVER a user-declared pin.
        if(exactPrivateBytes(originalFilteredCapturedRamPorts,
                             candidate.bytes,candidate.sha256,sourcePorts)) {
            realSession=&candidate;
            break;
        }
    }
    if(!realSession) {
        error="original 2026-11-13/14/15 Creative RAM ports original "
              "session length/SHA256 mismatch";
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
    if(!parseExactOriginalSourcePorts(sourcePorts,ports) ||
       ports.size()!=realSession->port_events) {
        error="original filtered RAM port syntax/length invalid";
        return false;
    }
    // NO guessed allocator/transform/loop. This is the EXACT 1996
    // Creative driver port capture. Accept legitimate rewrites only
    // when the FINAL ram matches source-observed words on all 260921.
    // The independently SHA-pinned source assets serve as the genuine
    // provenance for this ONE bank, not arbitrary external data.
    awe64::creative::P003RamUploadResult captured{};
    if(!awe64::creative::p003ReplaySourceCapturedMonoSf2BankAtObservedAddress(
           chip,ownerBank,ports,expectedFinal,true,captured,error) ||
       captured.physical_word_base!=kSourceObservedWordBase ||
       captured.original_port_writes!=ports.size() ||
       captured.original_sample_word_writes!=
           realSession->original_smlD_word_writes ||
       captured.source_pool_words!=kFinalOriginalSampleWords ||
       captured.sample_headers!=kOriginalMonoSampleHeaders ||
       captured.original_distinct_overwritten_words!=255u) {
        if(error.empty())
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
    // P003 integration into the REAL exported VST3 activation path.
    // This independent source->driver-transformed PCM16 converter is
    // allowed only for the same SHA-pinned owner bank AND authenticated
    // final Win95 physical RAM snapshot. It must agree with EVERY word
    // already deposited through authentic Win95 SMALW/SMLD port replay.
    // A disagreement is a hard activation failure, not a fallback to
    // guessed sample coordinates or a replacement DSP. Critically,
    // the verified 1996 SMALW/SMLD transfer must remain the SOLE
    // physical write path: do not overwrite it with a direct memcpy.
    awe64::creative::P003RamUploadResult p003{};
    std::string p003Error;
    if(!awe64::creative::p003VerifyReplayedOriginalWin95OwnerBankWithoutWrites(
           chip,ownerBank,observedFinalBytes,p003,p003Error) ||
       !p003.entire_pool_readback_equal ||
       !p003.original_creative_source_transformation_verified ||
       p003.physical_word_base!=kSourceObservedWordBase ||
       p003.source_pool_words!=kFinalOriginalSampleWords ||
       p003.sample_headers!=kOriginalMonoSampleHeaders) {
        error="P003 original source-converted VST3 RAM activation refused: "+
              p003Error;
        return false;
    }
    if(verified_capture)*verified_capture=captured;
    return true;
}
bool p003ReattestOwnerRamAfterOriginalPrelude(
    awe64::Chip& chip,const std::filesystem::path& privateOwnerSf2,
    const std::filesystem::path& observedOriginalFinalBankWordsPcm16,
    std::string& error) {
    error.clear();
    if(!chip.ready() || chip.nativeNoteOutTransactionActive()) {
        error="P003 original-prelude chip is not ready";return false;
    }
    std::vector<std::uint8_t> ownerBank,observedFinalBytes;
    if(!exactPrivateBytes(privateOwnerSf2,kSourceBankBytes,
                          kOwnerSf2Sha,ownerBank) ||
       !exactPrivateBytes(observedOriginalFinalBankWordsPcm16,
                          kFinalOriginalSampleWords*2u,
                          kOriginalFinalRamWordsSha,observedFinalBytes)) {
        error="P003 original-prelude SF2/final-RAM SHA mismatch";return false;
    }
    // No chip writes are allowed after the original Win95 prelude:
    // validate the full bank and transformation without disturbing
    // sample-RAM, chorus/EQ state, filters, or original chip timing.
    const auto* raw=chip.rawForContractTesting();
    if(!raw || !raw->ram) {
        error="P003 original-prelude RAM inaccessible";return false;
    }
    const std::size_t offset=kSourceObservedWordBase-awe64::kSampleRamBase;
    for(std::size_t i=0;i<kFinalOriginalSampleWords;++i) {
        const std::uint16_t expected=static_cast<std::uint16_t>(
            std::uint16_t(observedFinalBytes[2u*i]) |
            (std::uint16_t(observedFinalBytes[2u*i+1u])<<8u));
        if(static_cast<std::uint16_t>(raw->ram[offset+i])!=expected) {
            error="P003 original prelude RAM does not match original final bank";
            return false;
        }
    }
    awe64::creative::P003RamUploadResult p003{};
    if(!awe64::creative::p003VerifyReplayedOriginalWin95OwnerBankWithoutWrites(
           chip,ownerBank,observedFinalBytes,p003,error) ||
       !p003.entire_pool_readback_equal ||
       !p003.original_creative_source_transformation_verified ||
       p003.physical_word_base!=kSourceObservedWordBase ||
       p003.source_pool_words!=kFinalOriginalSampleWords ||
       p003.sample_headers!=kOriginalMonoSampleHeaders) {
        if(error.empty())error="P003 source-converted prelude sample proof failed";
        return false;
    }
    return true;
}
} // namespace awe64::vst3
