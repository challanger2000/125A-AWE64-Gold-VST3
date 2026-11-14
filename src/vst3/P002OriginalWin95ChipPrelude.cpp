#include "P002OriginalWin95ChipPrelude.h"
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
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string_view>
#include <vector>

namespace awe64::vst3 {
namespace {
constexpr std::size_t kExactTraceBytes=12506118u;
constexpr std::string_view kOriginalTraceSha=
    "eaaefc68a8421b20891f7e29216b16062686d721bab13c81ffd1c4ff52d73044";
constexpr std::uint32_t kOriginalC4Frame=1758037u;
constexpr std::uint32_t kExactOriginalPreludeWrites=676888u;
constexpr std::uint32_t kExactOriginalPreludeReads=49263u;
constexpr std::uint32_t kExpectedInitWrites=524u;
constexpr std::uint32_t kExpectedGlobalWrites=22u;
constexpr std::size_t kFinalBankWords=260921u;
constexpr std::uint32_t kFinalBankOffsetWords=16u;
constexpr std::string_view kOriginalRamSha=
    "ff3ce73a1898ef586312d368ad3abdc2cfad47e3cfe07dd6d460a32845d8c470";

bool sha(const std::vector<std::uint8_t>& data,
         const std::string_view expected) noexcept {
    if(data.empty() || data.size()>std::numeric_limits<ULONG>::max() ||
       expected.size()!=64u)return false;
    BCRYPT_ALG_HANDLE alg=nullptr;
    BCRYPT_HASH_HANDLE state=nullptr;
    std::array<UCHAR,32> hash{};
    bool good=false;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0) {
        if(BCryptCreateHash(alg,&state,nullptr,0,nullptr,0,0)==0) {
            if(BCryptHashData(state,const_cast<PUCHAR>(
                   reinterpret_cast<const UCHAR*>(data.data())),
                   static_cast<ULONG>(data.size()),0)==0 &&
               BCryptFinishHash(state,hash.data(),
                                static_cast<ULONG>(hash.size()),0)==0) {
                good=true;
                constexpr char hex[]="0123456789abcdef";
                for(unsigned i=0;i<hash.size();++i) {
                    if(hex[hash[i]>>4u]!=expected[i*2u] ||
                       hex[hash[i]&15u]!=expected[i*2u+1u])good=false;
                }
            }
            BCryptDestroyHash(state);
        }
        BCryptCloseAlgorithmProvider(alg,0);
    }
    return good;
}
bool loadPinned(const std::filesystem::path& path,
                std::size_t length,std::string_view digest,
                std::vector<std::uint8_t>& out) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in || in.tellg()!=static_cast<std::streamoff>(length))
        return false;
    out.resize(length);
    in.seekg(0);
    return bool(in.read(reinterpret_cast<char*>(out.data()),
                        static_cast<std::streamsize>(out.size()))) &&
           in.peek()==std::char_traits<char>::eof() &&
           sha(out,digest);
}
bool parse(const std::string& s,const int base,std::uint32_t& value) {
    if(s.empty())return false;
    const auto [p,err]=std::from_chars(s.data(),s.data()+s.size(),value,base);
    return err==std::errc{} && p==s.data()+s.size();
}
bool advanceTo(awe64::Chip& chip,const std::uint32_t clock,
               const std::uint32_t target) noexcept {
    if(target<clock || target>=kOriginalC4Frame)return false;
    std::array<std::int32_t,256u*2u> output{};
    std::uint32_t left=target-clock;
    while(left>0) {
        const auto count=std::min(left,256u);
        chip.render(output.data(),count);
        left-=count;
    }
    return true;
}
} // namespace

bool replayPrivateOriginalWin95ChipPrelude(
    awe64::Chip& chip,
    const std::filesystem::path& original,
    const std::filesystem::path& finalRam,
    std::uint32_t nativeC4,
    OriginalChipPreludeResult& report,
    std::string& error) {
    report={};error.clear();
    // P100 first C4 is exactly frame 137, not guessed. This owner-only
    // experiment is intentionally INELIGIBLE for other MIDI schedules.
    if(!chip.ready() || nativeC4!=137u) {
        error="prelude: original chip or P100 exact clock not available";return false;
    }
    std::vector<std::uint8_t> bytes,originalRam;
    if(!loadPinned(original,kExactTraceBytes,kOriginalTraceSha,bytes) ||
       !loadPinned(finalRam,kFinalBankWords*2u,kOriginalRamSha,originalRam)) {
        error="prelude: private original Win95 TRACE / final RAM SHA invalid";
        return false;
    }
    // Original chip clock at plugin activation must put original C4
    // at exactly host frame 137. Nothing after original C4 enters the
    // chip: those notes MUST still come from the real native MIDI engine.
    constexpr std::uint32_t kActivationChipClock=kOriginalC4Frame-137u;
    const std::string text(reinterpret_cast<const char*>(bytes.data()),
                           bytes.size());
    std::istringstream input(text);
    std::string line;
    std::uint32_t clock=0u,selector=0u;
    bool seenSourceHeader=false;
    while(std::getline(input,line)) {
        if(line.empty())continue;
        if(line[0]=='#') {
            if(line=="# EMU8000 port write trace, 44100 Hz timebase")
                seenSourceHeader=true;
            continue;
        }
        std::istringstream columns(line);
        std::string t,portText,valText,extra;
        if(!(columns>>t)) {error="prelude: empty event";return false;}
        bool read=t=="R";
        if(read && !(columns>>t)) {
            error="prelude: truncated read";return false;
        }
        if(!(columns>>portText>>valText) || columns>>extra) {
            error="prelude: malformed original port event";return false;
        }
        std::uint32_t frame=0u,port=0u,value=0u;
        if(!parse(t,10,frame) || !parse(portText,16,port) ||
           !parse(valText,16,value) ||
           value>65535u || frame<clock || frame>44100u*180u ||
           (port!=0xe22u && port!=0xe20u && port!=0xa20u &&
            port!=0xa22u && port!=0x620u && port!=0x622u)) {
            error="prelude: invalid source clock, port or value";return false;
        }
        if(frame>=kActivationChipClock)break; // absolutely NO source notes
        if(!advanceTo(chip,clock,frame)) {
            error="prelude: cannot advance original physical chip clock";
            return false;
        }
        clock=frame;
        if(read) {
            const auto observed=chip.readPort(static_cast<std::uint16_t>(port));
            ++report.physical_reads;
            if(observed!=value) {
                if(report.read_disagreements==0u)
                    report.first_read_disagreement_frame=frame;
                ++report.read_disagreements;
            }
        }else{
            if(port==0xe22u)selector=value;
            if((port==0xa20u||port==0xa22u) &&
               (selector>>5u)==2u+(port==0xa22u?0u:0u)) {
                // INIT1/INIT2 are A20/A22 register 2, respectively.
                ++report.source_init_writes;
            }else if((port==0xa20u||port==0xa22u) &&
                     (selector>>5u)==3u) {
                ++report.source_init_writes;
            }
            if((port==0xa20u || port==0xa22u) &&
               (selector>>5u)==1u) {
                const unsigned v=selector&31u;
                // HWCF4..7 are 32-bit A20/A22 writes. HWCF1..3
                // are 16-bit A20 writes; counting only A20 loses
                // eight original high halves (14 instead of 22).
                const bool fullConfig=(v==9u||v==10u||
                                       v==13u||v==14u);
                const bool shortConfig=(port==0xa20u &&
                                         (v==29u||v==30u||v==31u));
                if(fullConfig || shortConfig)
                    ++report.source_hardware_config_writes;
            }
            chip.writePort(static_cast<std::uint16_t>(port),
                           static_cast<std::uint16_t>(value));
            ++report.physical_writes;
        }
    }
    if(!seenSourceHeader ||
       report.physical_writes!=kExactOriginalPreludeWrites ||
       report.physical_reads!=kExactOriginalPreludeReads ||
       report.source_init_writes!=kExpectedInitWrites ||
       report.source_hardware_config_writes!=kExpectedGlobalWrites) {
        error="prelude: original Win95 hardware-init/read/write geometry mismatch";
        return false;
    }
    if(!advanceTo(chip,clock,kActivationChipClock)) {
        error="prelude: remaining original clock invalid";return false;
    }
    // The source is a strictly verified original full Win95 run, and
    // still must produce the INDEPENDENT final RAM PCM16 bank image.
    const auto* device=chip.rawForContractTesting();
    if(!device || !device->ram ||
       std::memcmp(device->ram+kFinalBankOffsetWords,
                   originalRam.data(),originalRam.size())!=0) {
        error="prelude: original Win95 RAM replay not byte-identical";
        return false;
    }
    // Reset ONLY the adapter's telemetry frame origin, NOT the actual
    // original EMU8000's filters, phase, WC, chorus, EQ or memory.
    if(!chip.rebaseDiagnosticFrameClock()) {
        error="prelude: native diagnostic frame clock reset refused";return false;
    }
    report.source_chip_clock_at_activation=kActivationChipClock;
    return true;
}
} // namespace awe64::vst3
