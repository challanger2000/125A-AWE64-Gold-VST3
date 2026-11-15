#include "CreativeSf2RamBankUpload.h"
#include "CreativeOriginalSmplThreeTap.h"
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
    const auto note=candidateOriginalSf2RamPhysicalNotePlan(
        sf2,bank,program,key,velocity,upload.physical_word_base,
        upload.source_pool_words,true);
    const auto* raw=chip.rawForContractTesting();
    if(!note || !raw || !raw->ram)return std::nullopt;
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
    for(std::size_t voice=0;voice<note->count;++voice) {
        const auto& v=note->layers[voice];
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
    return note;
}
} // namespace awe64::creative
