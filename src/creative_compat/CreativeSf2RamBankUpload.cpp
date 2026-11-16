#include "CreativeSf2RamBankUpload.h"
#include "CreativeOriginalSmplThreeTap.h"
#include <algorithm>
#include <bit>
#include <array>
#include <string_view>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#endif
#include <cstdint>
#include <limits>
#include <vector>
namespace awe64::creative {
namespace {
// The SHA-attested owner Creative captures contain legitimate RAM-capacity
// probes and exactly 16 zero prefix + 15 zero suffix guard writes outside
// the 260921 playable source words. Do not misidentify those as a
// different SF2 base or skip the original hardware writes.
// This narrow profile is activated by the OWNER caller only after the
// complete port file was checked against a pinned original-driver SHA.
enum class OwnerAuxProfile : std::uint8_t {
    none, guard31, probe57_and_guard31
};
constexpr std::uint32_t kOwnerBase=0x200010u;
constexpr std::size_t kOwnerWords=260921u;
constexpr std::uint32_t kOwnerEnd=kOwnerBase+kOwnerWords;
constexpr OwnerAuxProfile ownerAuxProfile(
    const std::size_t events,const std::size_t words,
    const bool sourceVerified) noexcept {
    if(!sourceVerified || words!=kOwnerWords)return OwnerAuxProfile::none;
    if(events==523182u)return OwnerAuxProfile::guard31;
    if(events==523467u)return OwnerAuxProfile::probe57_and_guard31;
    return OwnerAuxProfile::none;
}
constexpr std::size_t ownerAuxExpectedCount(
    const OwnerAuxProfile profile) noexcept {
    return profile==OwnerAuxProfile::probe57_and_guard31?88u:
        profile==OwnerAuxProfile::guard31?31u:0u;
}
constexpr bool ownerAuxExactWrite(
    const OwnerAuxProfile profile,const std::size_t n,
    const std::uint32_t address,const std::uint16_t word) noexcept {
    if(profile==OwnerAuxProfile::none ||
       n>=ownerAuxExpectedCount(profile))return false;
    std::size_t i=n;
    if(profile==OwnerAuxProfile::probe57_and_guard31) {
        if(i==0u)return address==0x200000u && word==0xaa55u;
        if(i==1u)return address==0x200000u && word==0x55aau;
        if(i<57u)return address==0x240000u+
            static_cast<std::uint32_t>(i-2u)*0x40000u &&
            word==0xaa55u;
        i-=57u;
    }
    if(i<16u)return address==0x200000u+i && word==0u;
    return address==kOwnerEnd+(i-16u) && word==0u;
}
static_assert(ownerAuxExactWrite(OwnerAuxProfile::probe57_and_guard31,
    56u,0xfc0000u,0xaa55u));
static_assert(ownerAuxExactWrite(OwnerAuxProfile::probe57_and_guard31,
    87u,0x23fb57u,0u));
static_assert(!ownerAuxExactWrite(OwnerAuxProfile::probe57_and_guard31,
    57u,0x200000u,1u)); // corrupted source guard must be rejected
static_assert(!ownerAuxExactWrite(OwnerAuxProfile::guard31,
    0u,0x200001u,0u));
bool p003CalculateSourceSha256(
    const std::span<const std::uint8_t> bytes,
    std::array<std::uint8_t,32>& digest) noexcept {
    digest.fill(0u);
#ifdef _WIN32
    if(bytes.empty() || bytes.size()>0xffffffffu)return false;
    BCRYPT_ALG_HANDLE provider=nullptr;
    BCRYPT_HASH_HANDLE handle=nullptr;
    bool ok=false;
    if(BCryptOpenAlgorithmProvider(
           &provider,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0) {
        if(BCryptCreateHash(provider,&handle,nullptr,0,nullptr,0,0)==0) {
            ok=BCryptHashData(handle,
                  const_cast<PUCHAR>(
                    reinterpret_cast<const UCHAR*>(bytes.data())),
                  static_cast<ULONG>(bytes.size()),0)==0 &&
                BCryptFinishHash(handle,digest.data(),
                    static_cast<ULONG>(digest.size()),0)==0;
            BCryptDestroyHash(handle);
        }
        BCryptCloseAlgorithmProvider(provider,0);
    }
    if(!ok)digest.fill(0u);
    return ok;
#else
    (void)bytes;
    return false;
#endif
}
}
bool p003CapturedRamMatchesExactSourceSf2(
    const std::span<const std::uint8_t> sf2,
    const P003RamUploadResult& capture) noexcept {
    if(!capture.original_source_port_replay_verified ||
       !capture.source_sf2_sha256_verified ||
       !capture.entire_pool_readback_equal)return false;
    std::array<std::uint8_t,32> digest{};
    return p003CalculateSourceSha256(sf2,digest) &&
           digest==capture.source_sf2_sha256;
}
bool p003CandidateAllocateAndUploadMonoSf2(
    awe64::Chip& chip,P003CandidateRamAllocator& allocator,
    const std::span<const std::uint8_t> sf2,
    P003CandidateRamAllocation& allocation,
    P003RamUploadResult& result,std::string& error) {
    result={};error.clear();
    // Never overwrite a still-owned lease: doing so would permanently
    // lose the caller's only handle and could recycle live voice RAM.
    if(allocation.words!=0u || allocation.generation!=0u ||
       allocation.base!=0u || allocation.id!=0u) {
        error="P003 caller allocation output still owns a bank lease";
        return false;
    }
    // Never reserve space for an unsupported SF2. This API is an
    // activation-thread candidate, not a realtime MIDI callback.
    if(!chip.ready() || chip.nativeNoteOutTransactionActive() ||
       !allocator.valid()) {
        error="P003 candidate allocator/chip not ready";
        return false;
    }
    const auto reservation=allocator.allocateMonoSf2(sf2);
    if(!reservation) {
        error="P003 unsupported SoundFont, no contiguous RAM space, "
              "or all 64 bounded allocation slots occupied";
        return false;
    }
    if(!p003UploadValidatedMonoSf2PoolToOriginalChip(
           chip,sf2,reservation->base,result,error)) {
        (void)allocator.release(*reservation);
        return false;
    }
    allocation=*reservation;
    return true;
}
bool p003UploadValidatedMonoSf2PoolToOriginalChip(
    awe64::Chip& chip,const std::span<const std::uint8_t> sf2,
    const std::uint32_t base,P003RamUploadResult& result,
    std::string& error) {
    result={};
    error.clear();
    if(!chip.ready() || chip.nativeNoteOutTransactionActive()) {
        error="P003 EMU8000 not initialized or active transaction";
        return false;
    }
    // Complete all structural checks BEFORE the first chip RAM write.
    // Keep four preceding physical words available for CCCA start-4;
    // this is a safety reservation, NOT a measured Creative allocator.
    const auto originalPool=candidateStandardSf2WholeMonoRamPool(sf2);
    if(!originalPool) {
        error="P003 SF2 not a complete, supported 16-bit RAM mono bank";
        return false;
    }
    const auto n=originalPool->whole_smpl_pcm_le16.size()/2u;
    if(base<awe64::kSampleRamBase+4u ||
       base>=awe64::kFmReservedBase ||
       n>std::size_t(awe64::kFmReservedBase-base)) {
        error="P003 SF2 pool exceeds 28-MiB physical RAM/reserved tail";
        return false;
    }
    const auto* raw=chip.rawForContractTesting();
    if(!raw || !raw->ram) {
        error="P003 original EMU8000 sample RAM inaccessible";
        return false;
    }
    // Convert signed 16-bit LE words exactly. Do NOT normalize PCM,
    // add guards, loop-pad or apply speculative 1996 Creative transforms.
    std::vector<std::int16_t> words;
    try {
        words.resize(n);
    }catch(...) {
        error="P003 sample pool allocation failed before chip writes";
        return false;
    }
    const auto b=originalPool->whole_smpl_pcm_le16;
    for(std::size_t i=0;i<n;++i) {
        const std::uint16_t unsignedWord=static_cast<std::uint16_t>(
            std::uint16_t(b[i*2u]) |
            (std::uint16_t(b[i*2u+1u])<<8u));
        words[i]=std::bit_cast<std::int16_t>(unsignedWord);
    }
    if(!chip.uploadRam(base,words)) {
        error="P003 EMU8000 rejected bounded sample pool upload";
        return false;
    }
    const auto offset=static_cast<std::size_t>(base-awe64::kSampleRamBase);
    for(std::size_t i=0;i<n;++i)
        if(raw->ram[offset+i]!=words[i]) {
            error="P003 real EMU8000 RAM readback differs from SF2 PCM16";
            return false;
        }
    result.physical_word_base=base;
    result.source_pool_words=n;
    result.sample_headers=originalPool->sample_headers;
    result.entire_pool_readback_equal=true;
    return true;
}
bool p003ReplaySourceCapturedMonoSf2Bank(
    awe64::Chip& chip,const std::span<const std::uint8_t> sf2,
    const std::span<const OriginalRamUploadPortWrite> ports,
    const std::uint32_t base,
    const std::span<const std::uint16_t> final_words,
    const bool source_verified,
    P003RamUploadResult& result,std::string& error) {
    result={};error.clear();
    if(!source_verified || !chip.ready() ||
       chip.nativeNoteOutTransactionActive()) {
        error="P003 source-captured original Creative driver provenance/EMU8000 gate";
        return false;
    }
    const auto pool=candidateStandardSf2WholeMonoRamPool(sf2);
    if(!pool || pool->whole_smpl_pcm_le16.size()/2u!=final_words.size() ||
       final_words.empty() || base<awe64::kSampleRamBase+4u ||
       base>=awe64::kFmReservedBase ||
       final_words.size()>std::size_t(awe64::kFmReservedBase-base)) {
        error="P003 source-captured SF2 pool/physical RAM geometry mismatch";
        return false;
    }
    // A missing SHA provider must reject BEFORE touching the emulated
    // Creative hardware. Never perform cryptographic source binding
    // after any original SMALW/SMLD write has already committed.
    std::array<std::uint8_t,32> bound_source_hash{};
    if(!p003CalculateSourceSha256(sf2,bound_source_hash)) {
        error="P003 original source bank SHA256 unavailable";
        return false;
    }
    // The 1996 upload trace may rewrite words, but MUST NOT touch any
    // physical word outside the separately authenticated final bank.
    // Preflight this BEFORE the first hardware-visible port write.
    std::uint16_t selected=0xffffu,low=0,high=0;
    bool haveLow=false,haveHigh=false,haveAddress=false;
    std::uint32_t address=0u;
    const auto profile=ownerAuxProfile(
        ports.size(),final_words.size(),source_verified);
    std::size_t auxiliaryWrites=0u;
    std::size_t writes=0u,bankWrites=0u;
    for(const auto& e:ports) {
        if(e.port==0xe22u) {
            if(e.value!=0x36u && e.value!=0x3au) {
                error="P003 source-captured non-RAM register selector";return false;
            }
            selected=e.value;
            if(selected==0x36u) {
                haveLow=false;haveHigh=false;haveAddress=false;
            }else if(!haveAddress) {
                error="P003 source-captured RAM write without pointer";return false;
            }
        }else if(selected==0x36u) {
            if(e.port==0xa20u) {low=e.value;haveLow=true;}
            else if(e.port==0xa22u) {high=e.value;haveHigh=true;}
            else {error="P003 source-captured malformed address port";return false;}
            if(haveLow && haveHigh) {
                address=std::uint32_t(low)|(std::uint32_t(high)<<16u);
                haveAddress=true;
            }
        }else if(selected==0x3au && e.port==0xa20u &&
                 haveAddress) {
            if(address<base || address-base>=final_words.size()) {
                if(profile==OwnerAuxProfile::none || base!=kOwnerBase ||
                   !ownerAuxExactWrite(
                       profile,auxiliaryWrites,address,e.value)) {
                    error="P003 original RAM outside SF2 is not an exact "
                          "verified Creative probe/guard";
                    return false;
                }
                ++auxiliaryWrites;
            }else ++bankWrites;
            ++writes;
            ++address;
        }else {
            error="P003 source-captured unexpected hardware port";return false;
        }
    }
    if(writes<final_words.size() ||
       bankWrites<final_words.size() ||
       auxiliaryWrites!=ownerAuxExpectedCount(profile)) {
        error="P003 source-captured missing RAM writes or probe/guard";
        return false;
    }
    // Last original SMLD write wins. Existing source oracle checks
    // EVERY final word and EVERY port BEFORE applying any writes.
    const auto replay=candidateReplayObservedOriginalCreativeFinalRamBank(
        chip,ports,base,final_words,true,true);
    if(!replay ||
       replay->independently_verified_final_word_count!=final_words.size() ||
       replay->original_port_upload.original_sample_words!=writes) {
        error="P003 original captured trace differs from final physical RAM";
        return false;
    }
    const auto* raw=chip.rawForContractTesting();
    if(!raw || !raw->ram) {
        error="P003 captured physical chip RAM readback unavailable";return false;
    }
    const auto start=std::size_t(base-awe64::kSampleRamBase);
    for(std::size_t i=0;i<final_words.size();++i) {
        if(std::bit_cast<std::uint16_t>(raw->ram[start+i])!=final_words[i]) {
            error="P003 original chip RAM readback mismatch";return false;
        }
    }
    result.physical_word_base=base;
    result.source_pool_words=final_words.size();
    result.sample_headers=pool->sample_headers;
    result.entire_pool_readback_equal=true;
    // This marks a checked original CAPTURE, not an independently
    // reconstructed Creative sample transform or allocator.
    result.original_creative_source_transformation_verified=false;
    result.original_port_writes=replay->original_port_upload.exact_port_writes;
    result.original_sample_word_writes=
        replay->original_port_upload.original_sample_words;
    result.original_distinct_overwritten_words=
        replay->source_sample_distinct_rewritten_words;
    result.original_source_port_replay_verified=true;
    result.source_sf2_sha256=bound_source_hash;
    result.source_sf2_sha256_verified=true;
    return true;
}
bool p003ReplaySourceCapturedMonoSf2BankAtObservedAddress(
    awe64::Chip& chip,const std::span<const std::uint8_t> sf2,
    const std::span<const OriginalRamUploadPortWrite> ports,
    const std::span<const std::uint16_t> final_words,
    const bool source_verified,
    P003RamUploadResult& result,std::string& error) {
    result={};error.clear();
    if(!source_verified || ports.empty() || final_words.empty()) {
        error="P003 no independently authenticated original Creative DRAM source";
        return false;
    }
    // Interpret ONLY the actual original Win95 SMALW/SMLD address
    // writes. Exact selector/order/clock checks are done by the common
    // original-port oracle before any hardware I/O is committed.
    std::uint16_t selected=0xffffu,lo=0u,hi=0u;
    bool low=false,high=false,pointer=false;
    std::uint32_t physical=0u;
    auto minAddress=std::numeric_limits<std::uint32_t>::max();
    const auto profile=ownerAuxProfile(
        ports.size(),final_words.size(),source_verified);
    std::size_t auxiliaryWrites=0u;
    std::size_t wordWrites=0u,bankWrites=0u;
    for(const auto& e:ports) {
        if(e.port==0xe22u) {
            selected=e.value;
            if(selected==0x36u) {
                low=false;high=false;pointer=false;
            }else if(selected!=0x3au || !pointer) {
                error="P003 observed address requires a complete SMALW";
                return false;
            }
        }else if(selected==0x36u) {
            if(e.port==0xa20u) {
                if(low) {error="P003 duplicate SMALW low word";return false;}
                lo=e.value;low=true;
            }else if(e.port==0xa22u) {
                if(high){error="P003 duplicate SMALW high word";return false;}
                hi=e.value;high=true;
            }else {
                error="P003 unexpected SMALW hardware port";return false;
            }
            if(low && high) {
                physical=std::uint32_t(lo)|(std::uint32_t(hi)<<16u);
                pointer=true;
            }
        }else if(selected==0x3au && pointer && e.port==0xa20u) {
            if(profile!=OwnerAuxProfile::none) {
                if(physical>=kOwnerBase &&
                   physical-kOwnerBase<final_words.size()) {
                    minAddress=std::min(minAddress,physical);
                    ++bankWrites;
                }else {
                    if(!ownerAuxExactWrite(
                           profile,auxiliaryWrites,physical,e.value)) {
                        error="P003 original probe/guard address or data "
                              "differs from independently attested source";
                        return false;
                    }
                    ++auxiliaryWrites;
                }
            }else {
                if(physical<awe64::kSampleRamBase+4u ||
                   physical>=awe64::kFmReservedBase) {
                    error="P003 original SMLD points into ROM/guard/FM tail";
                    return false;
                }
                minAddress=std::min(minAddress,physical);
                ++bankWrites;
            }
            ++wordWrites;
            ++physical;
        }else {
            error="P003 invalid original RAM address source event";
            return false;
        }
    }
    if(minAddress==std::numeric_limits<std::uint32_t>::max() ||
       bankWrites<final_words.size() ||
       auxiliaryWrites!=ownerAuxExpectedCount(profile) ||
       (profile!=OwnerAuxProfile::none && minAddress!=kOwnerBase)) {
        error="P003 original Creative RAM source lacks exact "
              "sample words or original probe/guard";
        return false;
    }
    // Deriving the address from the original 1996 driver *trace* is
    // NOT synthesizing the driver's still-unproven allocator.
    return p003ReplaySourceCapturedMonoSf2Bank(
        chip,sf2,ports,minAddress,final_words,source_verified,result,error);
}
bool p003CompareCandidateAllocationToOriginalCapture(
    P003CandidateRamAllocator& allocator,
    const std::span<const std::uint8_t> sf2,
    const P003RamUploadResult& observed,
    P003CandidateRamAllocation& verified_lease,
    std::string& error) {
    verified_lease={};error.clear();
    if(!p003CapturedRamMatchesExactSourceSf2(sf2,observed) ||
       !observed.original_source_port_replay_verified ||
       !allocator.valid()) {
        error="P003 original Creative comparison missing authentic matching SF2";
        return false;
    }
    const auto pool=candidateStandardSf2WholeMonoRamPool(sf2);
    if(!pool || pool->sample_headers!=observed.sample_headers ||
       pool->whole_smpl_pcm_le16.size()/2u!=observed.source_pool_words) {
        error="P003 source SF2 size/header differs from Creative trace";
        return false;
    }
    const auto allocated=allocator.allocateMonoSf2(sf2);
    if(!allocated) {
        error="P003 candidate allocator could not reserve original bank";
        return false;
    }
    if(allocated->base!=observed.physical_word_base ||
       allocated->words!=observed.source_pool_words) {
        // NO silent substitution of an arbitrary first-fit layout
        // for actual Creative WIN95 physical sample addresses.
        error="P003 candidate allocation disagrees with original Creative"
              " physical RAM base/size; Creative parity NOT proven";
        (void)allocator.release(*allocated);
        return false;
    }
    verified_lease=*allocated;
    return true;
}
std::optional<OriginalSf2RamPhysicalNotePlan>
p003CandidatePhysicalNoteFromOriginalSourceCapture(
    const awe64::Chip& chip,
    const std::span<const std::uint8_t> sf2,
    const P003RamUploadResult& capture,
    const std::span<const std::uint16_t> final_words,
    const std::uint16_t bank,const std::uint8_t program,
    const std::uint8_t key,const std::uint8_t velocity) noexcept {
    if(!chip.ready() || chip.nativeNoteOutTransactionActive() ||
       !capture.entire_pool_readback_equal ||
       !p003CapturedRamMatchesExactSourceSf2(sf2,capture) ||
       capture.original_port_writes==0u ||
       capture.original_sample_word_writes<capture.source_pool_words ||
       final_words.empty() ||
       final_words.size()!=capture.source_pool_words ||
       capture.physical_word_base<awe64::kSampleRamBase+4u ||
       capture.physical_word_base>=awe64::kFmReservedBase ||
       capture.source_pool_words>
          std::size_t(awe64::kFmReservedBase-capture.physical_word_base))
        return std::nullopt;
    const auto pool=candidateStandardSf2WholeMonoRamPool(sf2);
    if(!pool || pool->sample_headers!=capture.sample_headers ||
       pool->whole_smpl_pcm_le16.size()/2u!=final_words.size())
        return std::nullopt;
    const auto* raw=chip.rawForContractTesting();
    if(!raw || !raw->ram)return std::nullopt;
    const auto start=std::size_t(capture.physical_word_base-
                                  awe64::kSampleRamBase);
    // The captured Creative loader may transform every single PCM
    // word; comparing chip RAM to raw smpl would reject valid source.
    // Demand exact match against the WHOLE independent final snapshot.
    for(std::size_t i=0;i<final_words.size();++i)
        if(std::bit_cast<std::uint16_t>(raw->ram[start+i])!=final_words[i])
            return std::nullopt;
    return candidateOriginalSf2RamPhysicalNotePlan(
        sf2,bank,program,key,velocity,
        capture.physical_word_base,capture.source_pool_words,true);
}
#ifdef _WIN32
namespace {
bool matchesSourceSha256(
    const std::span<const std::uint8_t> bytes,
    const std::string_view expected_hex) noexcept {
    if(expected_hex.size()!=64u || bytes.size()>0xffffffffu)return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE handle=nullptr;
    bool ok=false;
    std::array<UCHAR,32> hash{};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,
                                   nullptr,0)==0) {
        if(BCryptCreateHash(algorithm,&handle,nullptr,0,nullptr,0,0)==0) {
            if(BCryptHashData(handle,
                const_cast<PUCHAR>(
                    reinterpret_cast<const UCHAR*>(bytes.data())),
                static_cast<ULONG>(bytes.size()),0)==0 &&
               BCryptFinishHash(handle,hash.data(),
                                static_cast<ULONG>(hash.size()),0)==0) {
                constexpr char hex[]="0123456789abcdef";
                ok=true;
                for(std::size_t i=0;i<hash.size();++i)
                    if(expected_hex[i*2u]!=hex[hash[i]>>4u] ||
                       expected_hex[i*2u+1u]!=hex[hash[i]&15u])
                        ok=false;
            }
            BCryptDestroyHash(handle);
        }
        BCryptCloseAlgorithmProvider(algorithm,0);
    }
    return ok;
}
}
#endif
bool p003UploadExactOriginalWin95ConvertedOwnerBank(
    awe64::Chip& chip,const std::span<const std::uint8_t> sf2,
    const std::span<const std::uint8_t> attested_ram,
    P003RamUploadResult& result,std::string& error) {
    result={};
    error.clear();
#ifndef _WIN32
    (void)chip;(void)sf2;(void)attested_ram;
    error="P003 original Creative source SHA profile requires Windows BCrypt";
    return false;
#else
    constexpr std::uint32_t base=0x200010u; // actual Win95 1996 VXD trace
    constexpr std::size_t n=260921u; // actual original VXD/SF2 RAM payload
    if(!chip.ready() || chip.nativeNoteOutTransactionActive() ||
       sf2.size()!=555754u || attested_ram.size()!=n*2u ||
       !matchesSourceSha256(sf2,
         "d5b99ec38fddf59f53f66b90921e170d207bf3bbd73960f6e4175c39ef68349b") ||
       !matchesSourceSha256(attested_ram,
         "ff3ce73a1898ef586312d368ad3abdc2cfad47e3cfe07dd6d460a32845d8c470")) {
        error="P003 owner SF2/Win95 original RAM SHA provenance invalid";
        return false;
    }
    const auto pool=candidateStandardSf2WholeMonoRamPool(sf2);
    if(!pool || pool->sample_headers!=83u ||
       pool->whole_smpl_pcm_le16.size()!=n*2u ||
       n>std::size_t(awe64::kFmReservedBase-base)) {
        error="P003 original owner bank sample structure or RAM bounds invalid";
        return false;
    }
    // Original Win95 2026-11-14 RAM oracle: floor((6*c-p-n)/8),
    // except >=3 consecutive zero runs; never use for arbitrary SF2s.
    const auto translated=p003PredictOwnerObservedThreeTapRamWords(
        pool->whole_smpl_pcm_le16);
    if(!translated || translated->size()!=n) {
        error="P003 original owner 3-tap driver model refused source bank";
        return false;
    }
    for(std::size_t i=0;i<n;++i) {
        const auto expectedWord=static_cast<std::uint16_t>(
            std::uint16_t(attested_ram[2u*i]) |
            (std::uint16_t(attested_ram[2u*i+1u])<<8u));
        if(std::bit_cast<std::uint16_t>((*translated)[i])!=expectedWord) {
            error="P003 converted source differs from genuine original RAM "
                  "BEFORE any physical chip write";
            return false;
        }
    }
    const auto* raw=chip.rawForContractTesting();
    if(!raw || !raw->ram || !chip.uploadRam(base,*translated)) {
        error="P003 exact observed driver bank physical RAM upload failed";
        return false;
    }
    const auto offset=std::size_t(base-awe64::kSampleRamBase);
    for(std::size_t i=0;i<n;++i)
        if(raw->ram[offset+i]!=(*translated)[i]) {
            error="P003 uploaded converted bank differs from original core RAM";
            return false;
        }
    result.physical_word_base=base;
    result.source_pool_words=n;
    result.sample_headers=pool->sample_headers;
    result.entire_pool_readback_equal=true;
    result.original_creative_source_transformation_verified=true;
    return true;
#endif
}
bool p003VerifyReplayedOriginalWin95OwnerBankWithoutWrites(
    const awe64::Chip& chip,const std::span<const std::uint8_t> sf2,
    const std::span<const std::uint8_t> attested_ram,
    P003RamUploadResult& result,std::string& error) {
    result={};error.clear();
#ifndef _WIN32
    (void)chip;(void)sf2;(void)attested_ram;
    error="P003 original source profile requires Windows BCrypt";
    return false;
#else
    constexpr std::uint32_t base=0x200010u;
    constexpr std::size_t n=260921u;
    if(!chip.ready() || chip.nativeNoteOutTransactionActive() ||
       sf2.size()!=555754u || attested_ram.size()!=n*2u ||
       !matchesSourceSha256(sf2,
         "d5b99ec38fddf59f53f66b90921e170d207bf3bbd73960f6e4175c39ef68349b") ||
       !matchesSourceSha256(attested_ram,
         "ff3ce73a1898ef586312d368ad3abdc2cfad47e3cfe07dd6d460a32845d8c470")) {
        error="P003 original owner source/physical snapshot SHA gate rejected";
        return false;
    }
    const auto pool=candidateStandardSf2WholeMonoRamPool(sf2);
    if(!pool || pool->sample_headers!=83u ||
       pool->whole_smpl_pcm_le16.size()!=2u*n ||
       n>std::size_t(awe64::kFmReservedBase-base)) {
        error="P003 original owner SF2 geometry invalid";return false;
    }
    const auto translated=p003PredictOwnerObservedThreeTapRamWords(
        pool->whole_smpl_pcm_le16);
    if(!translated || translated->size()!=n) {
        error="P003 original source transform unavailable";return false;
    }
    const auto* raw=chip.rawForContractTesting();
    if(!raw || !raw->ram) {
        error="P003 original core sample RAM unavailable";return false;
    }
    const auto offset=std::size_t(base-awe64::kSampleRamBase);
    for(std::size_t i=0;i<n;++i) {
        const auto expected=static_cast<std::uint16_t>(
            std::uint16_t(attested_ram[2u*i]) |
            (std::uint16_t(attested_ram[2u*i+1u])<<8u));
        if(std::bit_cast<std::uint16_t>((*translated)[i])!=expected) {
            error="P003 original source transform differs from SHA-pinned Win95 RAM";
            return false;
        }
        if(std::bit_cast<std::uint16_t>(raw->ram[offset+i])!=expected) {
            error="P003 original prelude chip RAM differs from Win95 RAM";
            return false;
        }
    }
    result.physical_word_base=base;
    result.source_pool_words=n;
    result.sample_headers=pool->sample_headers;
    result.entire_pool_readback_equal=true;
    result.original_creative_source_transformation_verified=true;
    return true;
#endif
}
std::optional<OriginalSf2RamPhysicalNotePlan>
p003CandidatePhysicalNoteFromVerifiedRawPool(
    const awe64::Chip& chip,const std::span<const std::uint8_t> sf2,
    const P003RamUploadResult& upload,const std::uint16_t bank,
    const std::uint8_t program,const std::uint8_t key,
    const std::uint8_t velocity) noexcept {
    // Do not accidentally claim original transformed-owner RAM parity:
    // that driver-transformed pool is NOT raw SF2 PCM.
    if(!chip.ready() || chip.nativeNoteOutTransactionActive() ||
       !upload.entire_pool_readback_equal ||
       upload.original_creative_source_transformation_verified ||
       upload.original_source_port_replay_verified ||
       upload.source_pool_words==0u ||
       upload.physical_word_base<awe64::kSampleRamBase+4u ||
       upload.physical_word_base>=awe64::kFmReservedBase ||
       upload.source_pool_words>
           std::size_t(awe64::kFmReservedBase-upload.physical_word_base))
        return std::nullopt;
    const auto pool=candidateStandardSf2WholeMonoRamPool(sf2);
    if(!pool || pool->sample_headers!=upload.sample_headers ||
       pool->whole_smpl_pcm_le16.size()/2u!=upload.source_pool_words)
        return std::nullopt;
    // P003 is a caller-addressed RAW sample upload, NOT an authenticated
    // Creative 1996 driver allocator. Never promote it by passing true
    // to candidateOriginalSf2RamPhysicalNotePlan(original_upload_verified).
    const auto source=candidateOriginalSf2RamMonoNotePcmPlan(
        sf2,bank,program,key,velocity,awe64::kSampleRamBytes/2u);
    const auto* raw=chip.rawForContractTesting();
    if(!source || source->count==0u || !raw || !raw->ram)
        return std::nullopt;
    OriginalSf2RamPhysicalNotePlan mapped{};
    mapped.count=source->count;
    for(std::size_t i=0;i<mapped.count;++i) {
        const auto& sample=source->layers[i];
        if(sample.total_pool_words!=upload.source_pool_words ||
           sample.start_word>=sample.end_word ||
           sample.end_word>upload.source_pool_words ||
           sample.loop_start_word<sample.start_word ||
           sample.loop_start_word>=sample.loop_end_word ||
           sample.loop_end_word>upload.source_pool_words)
            return std::nullopt;
        auto& dst=mapped.layers[i];
        dst.sample_id=sample.sample_id;
        dst.physical_sample_start=upload.physical_word_base+sample.start_word;
        dst.physical_sample_end=upload.physical_word_base+sample.end_word;
        dst.physical_loop_start=upload.physical_word_base+sample.loop_start_word;
        dst.physical_loop_end=upload.physical_word_base+sample.loop_end_word;
        dst.candidate_initial_ccca=dst.physical_sample_start-4u;
        dst.candidate_initial_psst=dst.physical_loop_start-1u;
        dst.candidate_initial_csl=dst.physical_loop_end-1u;
    }
    const auto bytes=pool->whole_smpl_pcm_le16;
    const auto offset=std::size_t(upload.physical_word_base-
                                  awe64::kSampleRamBase);
    // Verify the complete bank, not merely the currently requested layer.
    // A forged/stale upload result or corruption in another layer must
    // not be accepted as a verified physical SoundFont mapping.
    for(std::size_t word=0;word<upload.source_pool_words;++word) {
        const std::uint16_t original=static_cast<std::uint16_t>(
            std::uint16_t(bytes[word*2u]) |
            (std::uint16_t(bytes[word*2u+1u])<<8u));
        if(std::bit_cast<std::uint16_t>(raw->ram[offset+word])!=original)
            return std::nullopt;
    }
    for(std::size_t voice=0;voice<mapped.count;++voice) {
        const auto& v=mapped.layers[voice];
        const auto start=std::size_t(v.physical_sample_start-
                                    upload.physical_word_base);
        const auto end=std::size_t(v.physical_sample_end-
                                  upload.physical_word_base);
        if(start>=end || end>upload.source_pool_words)
            return std::nullopt;
        for(std::size_t word=start;word<end;++word) {
            const std::uint16_t original=static_cast<std::uint16_t>(
                std::uint16_t(bytes[word*2u]) |
                (std::uint16_t(bytes[word*2u+1u])<<8u));
            if(std::bit_cast<std::uint16_t>(raw->ram[offset+word])!=original)
                return std::nullopt;
        }
    }
    return mapped;
}
} // namespace awe64::creative
