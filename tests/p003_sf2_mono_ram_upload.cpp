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
    // One playable SF2 preset, program 7, instrument 0, sample 0.
    // This allows an actual MIDI->sample->physical-chip-address test.
    Bytes phdr,pbag,pgen,inst,ibag,igen;
    name(phdr,"P003-PRESET");
    w16(phdr,7u);w16(phdr,0u);w16(phdr,0u);
    w32(phdr,0u);w32(phdr,0u);w32(phdr,0u);
    name(phdr,"EOP");w16(phdr,0u);w16(phdr,0u);w16(phdr,1u);
    w32(phdr,0u);w32(phdr,0u);w32(phdr,0u);
    w16(pbag,0u);w16(pbag,0u);
    w16(pbag,1u);w16(pbag,0u);
    w16(pgen,41u);w16(pgen,0u);
    name(inst,"P003-INSTRUMENT");w16(inst,0u);
    name(inst,"EOI");w16(inst,1u);
    w16(ibag,0u);w16(ibag,0u);
    w16(ibag,1u);w16(ibag,0u);
    w16(igen,53u);w16(igen,0u);
    chunk(pdta,"phdr",phdr);
    chunk(pdta,"pbag",pbag);
    chunk(pdta,"pgen",pgen);
    chunk(pdta,"inst",inst);
    chunk(pdta,"ibag",ibag);
    chunk(pdta,"igen",igen);
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
    // Physically verify the RAM bytes for the selected SF2 MIDI
    // NoteOn before exposing CCCA/PSST/CSL chip word addresses.
    const auto note=p003CandidatePhysicalNoteFromVerifiedRawPool(
        chip,bank,outcome,0u,7u,60u,100u);
    CHECK(note && note->count==1u);
    CHECK(note->layers[0].sample_id==0u);
    CHECK(note->layers[0].physical_sample_start==first);
    CHECK(note->layers[0].physical_sample_end==first+4u);
    CHECK(note->layers[0].candidate_initial_ccca==first-4u);
    CHECK(note->layers[0].candidate_initial_psst==first);
    CHECK(note->layers[0].candidate_initial_csl==first+2u);
    CHECK(!p003CandidatePhysicalNoteFromVerifiedRawPool(
        chip,bank,outcome,0u,6u,60u,100u));
    auto forged=outcome;
    forged.entire_pool_readback_equal=false;
    CHECK(!p003CandidatePhysicalNoteFromVerifiedRawPool(
        chip,bank,forged,0u,7u,60u,100u));
    forged=outcome;
    forged.original_creative_source_transformation_verified=true;
    CHECK(!p003CandidatePhysicalNoteFromVerifiedRawPool(
        chip,bank,forged,0u,7u,60u,100u));
    auto damaged=bank;
    // Same geometry, different PCM source: the memory proof must fail.
    const auto poolPosition=static_cast<std::size_t>(
        whole->whole_smpl_pcm_le16.data()-bank.data());
    damaged[poolPosition]^=1u;
    CHECK(!p003CandidatePhysicalNoteFromVerifiedRawPool(
        chip,damaged,outcome,0u,7u,60u,100u));
    // Corrupt a DIFFERENT physical sample while playing sample 0:
    // source-wide verification must still reject the bank.
    auto* rawPcm=chip.rawForContractTesting();
    CHECK(rawPcm && rawPcm->ram);
    const auto untouchedSecond=rawPcm->ram[
        first-awe64::kSampleRamBase+5u];
    rawPcm->ram[first-awe64::kSampleRamBase+5u]^=1;
    CHECK(!p003CandidatePhysicalNoteFromVerifiedRawPool(
        chip,bank,outcome,0u,7u,60u,100u));
    rawPcm->ram[first-awe64::kSampleRamBase+5u]=untouchedSecond;
    CHECK(p003CandidatePhysicalNoteFromVerifiedRawPool(
        chip,bank,outcome,0u,7u,60u,100u));
    const std::array<std::uint16_t,8> expected{
        0xffffu,0x8000u,0x0000u,0x7fffu,
        0x1234u,0x8001u,0xa55au,0x0000u};
    const auto at=first-awe64::kSampleRamBase;
    for(std::size_t i=0;i<8u;++i)
        CHECK(static_cast<std::uint16_t>(original->ram[at+i])==expected[i]);
    // A new successful load at exactly the last eight permitted words
    // may not alter the thirty-two reserved FM words 0xffffe0..ffffff.
    constexpr std::uint32_t lastAllowed=awe64::kFmReservedBase-8u;
    // Preserve and compare the ENTIRE reserved FM tail, not only its
    // first word: an out-of-bounds transfer can corrupt a later word.
    constexpr std::size_t fmTailWords=32u;
    std::array<std::int16_t,fmTailWords> reservedBefore{};
    for(std::size_t i=0;i<fmTailWords;++i)
        reservedBefore[i]=original->ram[
            awe64::kFmReservedBase-awe64::kSampleRamBase+i];
    CHECK(p003UploadValidatedMonoSf2PoolToOriginalChip(
        chip,bank,lastAllowed,outcome,error));
    CHECK(outcome.source_pool_words==8u &&
          outcome.physical_word_base==lastAllowed);
    for(std::size_t i=0;i<8u;++i)
        CHECK(static_cast<std::uint16_t>(
            original->ram[lastAllowed-awe64::kSampleRamBase+i])==expected[i]);
    for(std::size_t i=0;i<fmTailWords;++i)
        CHECK(original->ram[
            awe64::kFmReservedBase-awe64::kSampleRamBase+i]==
            reservedBefore[i]);
    // General P003 driver-CAPTURED loader: different physical base,
    // independent final words and original Creative SMALW/SMLD register
    // writes. This models a second source recording, never a guessed
    // 1996 allocator or generic automatic 3-tap source transform.
    constexpr auto recordedBase=std::uint32_t(awe64::kSampleRamBase+128u);
    std::vector<OriginalRamUploadPortWrite> captured;
    const auto port=[&](std::uint16_t p,std::uint16_t v) {
        captured.push_back({static_cast<std::uint64_t>(captured.size()),
                            p,v});
    };
    port(0xe22u,0x36u);
    port(0xa20u,static_cast<std::uint16_t>(recordedBase));
    port(0xa22u,static_cast<std::uint16_t>(recordedBase>>16u));
    port(0xe22u,0x3au);
    for(const auto word:expected)port(0xa20u,word);
    P003RamUploadResult capturedResult{};
    std::vector<std::uint16_t> originalFinal(
        expected.begin(),expected.end());
    const auto capturedIndex=recordedBase-awe64::kSampleRamBase;
    // Caller-provided source provenance flag is mandatory. A failure
    // must leave entire destination unchanged.
    const auto beforeCaptured=original->ram[capturedIndex];
    CHECK(!p003ReplaySourceCapturedMonoSf2Bank(
        chip,bank,captured,recordedBase,originalFinal,false,
        capturedResult,error));
    CHECK(original->ram[capturedIndex]==beforeCaptured);
    auto invalidFinal=originalFinal;
    invalidFinal[7u]^=1u;
    CHECK(!p003ReplaySourceCapturedMonoSf2Bank(
        chip,bank,captured,recordedBase,invalidFinal,true,
        capturedResult,error));
    CHECK(original->ram[capturedIndex]==beforeCaptured);
    // All 8 source values must match at a different RAM location.
    CHECK(p003ReplaySourceCapturedMonoSf2Bank(
        chip,bank,captured,recordedBase,originalFinal,true,
        capturedResult,error));
    CHECK(capturedResult.entire_pool_readback_equal &&
          capturedResult.original_source_port_replay_verified &&
          capturedResult.original_port_writes==captured.size() &&
          capturedResult.original_sample_word_writes==8u &&
          !capturedResult.original_creative_source_transformation_verified &&
          capturedResult.physical_word_base==recordedBase &&
          capturedResult.source_pool_words==8u &&
          capturedResult.sample_headers==2u);
    CHECK(capturedResult.source_sf2_sha256_verified);
    CHECK(p003CapturedRamMatchesExactSourceSf2(bank,capturedResult));
    // Even if captured original RAM happens to equal raw SF2 bytes,
    // it MUST never be silently reclassified as an unauthenticated
    // direct-copy bank through the other candidate mapping API.
    CHECK(!p003CandidatePhysicalNoteFromVerifiedRawPool(
        chip,bank,capturedResult,0u,7u,60u,100u));
    // A different bank with identical sample headers/PCM pool geometry
    // cannot borrow another Creative driver's physical RAM attestation.
    auto differentBank=bank;
    differentBank[poolPosition]^=1u;
    CHECK(!p003CapturedRamMatchesExactSourceSf2(
        differentBank,capturedResult));
    CHECK(!p003CandidatePhysicalNoteFromOriginalSourceCapture(
        chip,differentBank,capturedResult,originalFinal,0u,7u,60u,100u));
    auto forgedCapture=capturedResult;
    forgedCapture.source_sf2_sha256_verified=false;
    CHECK(!p003CapturedRamMatchesExactSourceSf2(bank,forgedCapture));
    const auto mappedCaptured=p003CandidatePhysicalNoteFromOriginalSourceCapture(
        chip,bank,capturedResult,originalFinal,0u,7u,60u,100u);
    CHECK(mappedCaptured && mappedCaptured->count==1u);
    CHECK(mappedCaptured->layers[0].physical_sample_start==recordedBase);
    CHECK(mappedCaptured->layers[0].physical_sample_end==recordedBase+4u);
    CHECK(mappedCaptured->layers[0].candidate_initial_ccca==recordedBase-4u);
    auto staleCaptured=originalFinal;
    staleCaptured[7u]^=1u;
    CHECK(!p003CandidatePhysicalNoteFromOriginalSourceCapture(
        chip,bank,capturedResult,staleCaptured,0u,7u,60u,100u));
    CHECK(!p003CandidatePhysicalNoteFromOriginalSourceCapture(
        chip,bank,capturedResult,originalFinal,0u,6u,60u,100u));
    // Corruption in a DIFFERENT sample must invalidate a note for
    // sample0. A per-note-only RAM check misses this bank binding bug.
    const auto otherSampleWord=original->ram[capturedIndex+5u];
    original->ram[capturedIndex+5u]^=1;
    CHECK(!p003CandidatePhysicalNoteFromOriginalSourceCapture(
        chip,bank,capturedResult,originalFinal,0u,7u,60u,100u));
    original->ram[capturedIndex+5u]=otherSampleWord;
    CHECK(p003CandidatePhysicalNoteFromOriginalSourceCapture(
        chip,bank,capturedResult,originalFinal,0u,7u,60u,100u));
    for(std::size_t i=0;i<expected.size();++i)
        CHECK(std::uint16_t(original->ram[capturedIndex+i])==expected[i]);
    // No original driver source recording is authorized to overwrite
    // another bank or protected physical RAM. Reject before first OUT.
    auto escaped=captured;
    escaped[1u].value=static_cast<std::uint16_t>(recordedBase+64u);
    CHECK(!p003ReplaySourceCapturedMonoSf2Bank(
        chip,bank,escaped,recordedBase,originalFinal,true,
        capturedResult,error));
    for(std::size_t i=0;i<expected.size();++i)
        CHECK(std::uint16_t(original->ram[capturedIndex+i])==expected[i]);
    // The original Creative driver may rewrite an already uploaded
    // word. Last value must win; the final bank must still match.
    auto repeatedCapture=captured;
    const auto add=[&](std::uint16_t p,std::uint16_t v) {
        repeatedCapture.push_back({
            static_cast<std::uint64_t>(repeatedCapture.size()),p,v});
    };
    add(0xe22u,0x36u);
    add(0xa20u,static_cast<std::uint16_t>(recordedBase+3u));
    add(0xa22u,static_cast<std::uint16_t>(recordedBase>>16u));
    add(0xe22u,0x3au);
    add(0xa20u,expected[3u]);
    CHECK(p003ReplaySourceCapturedMonoSf2Bank(
        chip,bank,repeatedCapture,recordedBase,originalFinal,true,
        capturedResult,error));
    CHECK(capturedResult.entire_pool_readback_equal);
    // Real Creative can transform PCM on upload. This test changes a
    // captured SMLD value AND its independent final RAM snapshot while
    // leaving the source SF2 untouched. The RAW upload resolver must
    // refuse it; the original CAPTURE resolver must still use exactly
    // the measured physical chip address and full final image.
    auto convertedPorts=captured;
    convertedPorts[6u].value=0x1234u; // source sample 2 was 0x0000
    auto convertedFinal=originalFinal;
    convertedFinal[2u]=0x1234u;
    P003RamUploadResult convertedCapture{};
    CHECK(p003ReplaySourceCapturedMonoSf2Bank(
        chip,bank,convertedPorts,recordedBase,convertedFinal,true,
        convertedCapture,error));
    CHECK(convertedCapture.original_source_port_replay_verified &&
          !convertedCapture.original_creative_source_transformation_verified);
    CHECK(std::uint16_t(original->ram[capturedIndex+2u])==0x1234u);
    const auto convertedNote=p003CandidatePhysicalNoteFromOriginalSourceCapture(
        chip,bank,convertedCapture,convertedFinal,0u,7u,60u,100u);
    CHECK(convertedNote && convertedNote->count==1u &&
          convertedNote->layers[0].physical_sample_start==recordedBase);
    CHECK(!p003CandidatePhysicalNoteFromVerifiedRawPool(
        chip,bank,convertedCapture,0u,7u,60u,100u));
    // Restore genuine synthetic fixture so later negative checks also
    // validate that this test never leaves an altered sample behind.
    CHECK(p003ReplaySourceCapturedMonoSf2Bank(
        chip,bank,captured,recordedBase,originalFinal,true,
        capturedResult,error));
    CHECK(std::uint16_t(original->ram[capturedIndex+2u])==expected[2u]);
    // A synthetic, unauthenticated bank can NEVER activate the
    // source-attested Creative 1996 transform+upload path.
    P003RamUploadResult fakeOriginal{};
    const std::array<std::uint8_t,8> forgedRam{};
    CHECK(!p003UploadExactOriginalWin95ConvertedOwnerBank(
        chip,bank,forgedRam,fakeOriginal,error));
    CHECK(!error.empty() && !fakeOriginal.entire_pool_readback_equal &&
          !fakeOriginal.original_creative_source_transformation_verified);
    // P003 original-prelude attestation is a VERIFY-ONLY entrypoint.
    // Public CI has no private Creative source, so synthetic inputs
    // MUST fail before any physical chip write and must not change
    // even one word of the existing loaded bank or the chip clock.
    const auto verifyClock=chip.renderedFrames();
    const auto verifyFirst=original->ram[at];
    const auto verifyLast=original->ram[
        lastAllowed-awe64::kSampleRamBase+7u];
    P003RamUploadResult rejectedPrelude{
        999u,7u,5u,true,true};
    error.clear();
    CHECK(!p003VerifyReplayedOriginalWin95OwnerBankWithoutWrites(
        chip,bank,forgedRam,rejectedPrelude,error));
    CHECK(!error.empty() && !rejectedPrelude.entire_pool_readback_equal &&
          !rejectedPrelude.original_creative_source_transformation_verified &&
          rejectedPrelude.physical_word_base==0u &&
          rejectedPrelude.source_pool_words==0u);
    CHECK(chip.renderedFrames()==verifyClock &&
          original->ram[at]==verifyFirst &&
          original->ram[lastAllowed-awe64::kSampleRamBase+7u]==verifyLast);
    // Every rejected input must leave BOTH earlier valid sample
    // destinations AND the protected tail untouched. Checking just the
    // first sample word would miss partial corruptions after it.
    std::array<std::int16_t,8> firstBefore{},lastBefore{};
    for(std::size_t i=0;i<8u;++i) {
        firstBefore[i]=original->ram[at+i];
        lastBefore[i]=original->ram[
            lastAllowed-awe64::kSampleRamBase+i];
    }
    auto reject=[&](std::span<const std::uint8_t> broken,
                    std::uint32_t physicalBase) {
        P003RamUploadResult bad{123u,7u,1u,true};
        error.clear();
        const bool accepted=p003UploadValidatedMonoSf2PoolToOriginalChip(
            chip,broken,physicalBase,bad,error);
        if(accepted || error.empty() || bad.entire_pool_readback_equal)
            return false;
        for(std::size_t i=0;i<8u;++i)
            if(original->ram[at+i]!=firstBefore[i] ||
               original->ram[lastAllowed-awe64::kSampleRamBase+i]!=lastBefore[i])
                return false;
        for(std::size_t i=0;i<fmTailWords;++i)
            if(original->ram[
                  awe64::kFmReservedBase-awe64::kSampleRamBase+i]!=
               reservedBefore[i])
                return false;
        return true;
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
        <<" generic_original_captured_port_upload=TRUE"
        <<" captured_sf2_SHA256_misbinding_rejected=TRUE"
        <<" original_creative_allocator=NOT_CLAIMED"
        <<" external_proprietary_assets=NONE\n";
    return 0;
}
