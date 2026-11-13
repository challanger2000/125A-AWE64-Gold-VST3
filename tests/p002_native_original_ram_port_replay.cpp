#include "CreativeOriginalRamPortReplay.h"
#include <86box/sound.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
using awe64::creative::OriginalRamUploadPortWrite;
using awe64::creative::candidateReplayOriginalCreativeCapturedRamUpload;
#define CHECK(x) do { if(!(x)) { \
    std::cerr<<"P002_RAM_PORT_REPLAY_FAIL line "<<__LINE__ \
             <<" condition "#x"\n";return 1; } } while(false)
namespace {
using Writes=std::vector<OriginalRamUploadPortWrite>;
void setPointer(Writes& trace,std::uint64_t frame,std::uint32_t at) {
    trace.push_back({frame,0xe22u,0x36u});
    trace.push_back({frame,0xa20u,static_cast<std::uint16_t>(at)});
    trace.push_back({frame,0xa22u,static_cast<std::uint16_t>(at>>16u)});
    trace.push_back({frame,0xe22u,0x3au});
}
void addWords(Writes& trace,std::uint64_t frame,
              std::uint32_t at,std::span<const std::uint16_t> words) {
    setPointer(trace,frame,at);
    for(const auto data:words)
        trace.push_back({frame++,0xa20u,data});
}
void put16(std::vector<std::uint8_t>& b,const std::uint16_t v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v>>8u));
}
void put32(std::vector<std::uint8_t>& b,const std::uint32_t v) {
    put16(b,static_cast<std::uint16_t>(v));
    put16(b,static_cast<std::uint16_t>(v>>16u));
}
void putChunk(std::vector<std::uint8_t>& out,
              const std::string_view tag,
              const std::vector<std::uint8_t>& bytes) {
    out.insert(out.end(),tag.begin(),tag.end());
    put32(out,static_cast<std::uint32_t>(bytes.size()));
    out.insert(out.end(),bytes.begin(),bytes.end());
    if(bytes.size()&1u)out.push_back(0u);
}
void putName(std::vector<std::uint8_t>& out,
             const std::string_view name) {
    out.insert(out.end(),name.begin(),name.end());
    out.insert(out.end(),20u-name.size(),0u);
}
std::vector<std::uint8_t> originalRamSf2SampleFixture(
    const std::array<std::uint16_t,4>& source,
    const bool mutatedSample=false,
    const std::uint16_t sampleType=1u) {
    // Synthetic, format-2 46-byte shdr, signed-16 SDTA PCM16.
    // No real Creative sample source bytes are included.
    std::vector<std::uint8_t> smpl{};
    for(std::uint16_t i=0u;i<16u;++i) {
        auto word=(i>=4u && i<8u)?source[i-4u]:i*31u;
        if(mutatedSample && i==7u)word^=1u;
        put16(smpl,word);
    }
    std::vector<std::uint8_t> shdr{};
    putName(shdr,"RAM SOURCE");
    put32(shdr,4u);put32(shdr,8u);
    put32(shdr,5u);put32(shdr,7u);
    put32(shdr,44100u);
    shdr.push_back(60u);shdr.push_back(0u);
    put16(shdr,0u);put16(shdr,sampleType);
    putName(shdr,"EOS");
    for(unsigned i=0;i<5u;++i)put32(shdr,0u);
    shdr.push_back(0u);shdr.push_back(0u);
    put16(shdr,0u);put16(shdr,0u);
    std::vector<std::uint8_t> info{'I','N','F','O'};
    putChunk(info,"ifil",{2u,0u,0u,0u});
    std::vector<std::uint8_t> sdta{'s','d','t','a'};
    putChunk(sdta,"smpl",smpl);
    std::vector<std::uint8_t> pdta{'p','d','t','a'};
    putChunk(pdta,"shdr",shdr);
    std::vector<std::uint8_t> body{'s','f','b','k'};
    putChunk(body,"LIST",info);
    putChunk(body,"LIST",sdta);
    putChunk(body,"LIST",pdta);
    std::vector<std::uint8_t> sf2{'R','I','F','F'};
    put32(sf2,static_cast<std::uint32_t>(body.size()));
    sf2.insert(sf2.end(),body.begin(),body.end());
    return sf2;
}
}
int main() {
    const auto rom=std::filesystem::temp_directory_path()/
        "125a-p002-original-RAM-port-replay-synthetic-ROM.raw";
    {
        std::ofstream file(rom,std::ios::binary|std::ios::trunc);
        CHECK(bool(file));
        const std::array<char,4096> empty{};
        for(int i=0;i<256;++i)file.write(empty.data(),empty.size());
        CHECK(bool(file));
    }
    awe64::Chip chip;
    std::string error;
    CHECK(chip.open(rom,error));
    std::error_code ec;std::filesystem::remove(rom,ec);
    auto* core=chip.rawForContractTesting();
    CHECK(core!=nullptr && core->ram!=nullptr);
    const std::array<std::uint16_t,4> data{
        {0x8001u,0x7fffu,0x00ffu,0xabcdu}};
    Writes traced{};
    addWords(traced,5u,0x200010u,data);
    const std::array<std::uint16_t,1> last{{0x55aau}};
    addWords(traced,11u,0xffffdfu,last);
    const auto before=std::array<std::uint16_t,5>{{
        static_cast<std::uint16_t>(core->ram[0x10u]),
        static_cast<std::uint16_t>(core->ram[0x11u]),
        static_cast<std::uint16_t>(core->ram[0x12u]),
        static_cast<std::uint16_t>(core->ram[0x13u]),
        static_cast<std::uint16_t>(
            core->ram[0xffffdfu-awe64::kSampleRamBase])}};
    // Synthetic public test trace is NOT independently authenticated;
    // FALSE must NEVER write or accept it as proven original Win95.
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,traced,false));
    CHECK(static_cast<std::uint16_t>(core->ram[0x10u])==before[0]);
    // Negative original trace with otherwise valid first RAM word, but
    // second segment crosses the EMU8000 FM-reserved high memory gate.
    Writes overflow=traced;
    overflow.push_back({14u,0xa20u,0xdead});
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,overflow,true));
    CHECK(static_cast<std::uint16_t>(core->ram[0x10u])==before[0]);
    CHECK(static_cast<std::uint16_t>(
        core->ram[0xffffdfu-awe64::kSampleRamBase])==before[4]);
    // Missing high address, wrong selector, wrong DATA port, duplicate
    // low, backwards original chip clock, and forged non-RAM pointer
    // are each rejected before any pin-unmodified-chip port effects.
    auto invalid=traced;
    invalid.erase(invalid.begin()+2);
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    invalid=traced;invalid[0].value=0x37u;
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    invalid=traced;invalid[4].port=0xa22u;
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    invalid=traced;invalid.insert(invalid.begin()+2,
                                  {5u,0xa20u,0x0010u});
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    invalid=traced;invalid[5].original_chip_frame=0u;
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    invalid=traced;invalid[1].value=0xffffu;invalid[2].value=0x001fu;
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    invalid=traced;invalid[9].value=0xffe0u;
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    invalid=traced;
    invalid.pop_back();invalid.pop_back();invalid.pop_back();
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true)); // dangling pointer before SMLD
    // The source Python scanner already rejects interrupted SMALW.
    // Native replay must likewise reject before ANY physical chip write,
    // including when a later completely valid upload could hide it.
    for(unsigned n=0u;n<3u;++n) {
        invalid=traced;
        const std::size_t switchAt=1u+n;
        invalid.insert(invalid.begin()+switchAt,
                       {5u,0xe22u,0x36u});
        CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
            chip,invalid,true)); // 0, 1 or 2 pointer halves skipped
        CHECK(static_cast<std::uint16_t>(core->ram[0x10u])==before[0]);
    }
    // No original SMLD may select data without a full 32-bit pointer;
    // even if a later segment would otherwise be source-valid.
    invalid=traced;
    invalid.insert(invalid.begin()+2u,{5u,0xe22u,0x3au});
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    CHECK(static_cast<std::uint16_t>(core->ram[0x10u])==before[0]);
    // Invalid SMALW data ports must not be ignored as incidental events.
    invalid=traced;invalid[1u].port=0xa24u;
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    CHECK(static_cast<std::uint16_t>(core->ram[0x10u])==before[0]);
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,{},true));
    chip.close();
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,traced,true)); // no chip allocated
    CHECK(chip.open(std::filesystem::temp_directory_path()/
                    "125a-p002-original-RAM-port-replay-synthetic-ROM.raw",
                    error)==false); // already deleted original fixture
    // Re-create an original-fork chip with an explicitly SYNTHETIC ROM
    // and verify actual, typed low/high EMU8000 port data, NOT memcpy.
    {
        std::ofstream file(rom,std::ios::binary|std::ios::trunc);
        CHECK(bool(file));
        const std::array<char,4096> empty{};
        for(int i=0;i<256;++i)file.write(empty.data(),empty.size());
        CHECK(bool(file));
    }
    CHECK(chip.open(rom,error));
    std::filesystem::remove(rom,ec);
    core=chip.rawForContractTesting();
    const auto done=candidateReplayOriginalCreativeCapturedRamUpload(
        chip,traced,true);
    CHECK(done && done->exact_port_writes==traced.size() &&
          done->original_sample_words==5u &&
          done->first_word_address==0x200010u &&
          done->last_word_address==0xffffdfu &&
          done->first_chip_frame==5u &&
          done->last_chip_frame==11u);
    for(unsigned i=0;i<4u;++i)
        CHECK(static_cast<std::uint16_t>(core->ram[0x10u+i])==data[i]);
    CHECK(static_cast<std::uint16_t>(
        core->ram[0xffffdfu-awe64::kSampleRamBase])==0x55aau);
    // Full source SAMPLE ↔ original captured port replay bridge.
    // This is a synthetic Win95-shaped input, NOT real Creative VXD
    // provenance. It proves byte-for-byte sample match at exactly the
    // captured physical address and fail-closed before original chip
    // mutation on the very LAST original sample word.
    const auto bank=originalRamSf2SampleFixture(data);
    const auto mutatedBank=originalRamSf2SampleFixture(data,true);
    const auto romBank=originalRamSf2SampleFixture(data,false,0x8001u);
    CHECK(!awe64::creative::candidateReplaySourceMatchedOriginalRamSf2Sample(
        chip,bank,0u,0x200010u,traced,false));
    CHECK(!awe64::creative::candidateReplaySourceMatchedOriginalRamSf2Sample(
        chip,mutatedBank,0u,0x200010u,traced,true));
    CHECK(!awe64::creative::candidateReplaySourceMatchedOriginalRamSf2Sample(
        chip,romBank,0u,0x200010u,traced,true));
    CHECK(!awe64::creative::candidateReplaySourceMatchedOriginalRamSf2Sample(
        chip,bank,1u,0x200010u,traced,true)); // EOS cannot be RAM
    CHECK(!awe64::creative::candidateReplaySourceMatchedOriginalRamSf2Sample(
        chip,bank,0u,0x20000fu,traced,true)); // guessed wrong base
    CHECK(!awe64::creative::candidateReplaySourceMatchedOriginalRamSf2Sample(
        chip,bank,0u,0xffffdeu,traced,true)); // FM reserved overlap
    auto shortSampleTrace=traced;
    shortSampleTrace.erase(shortSampleTrace.begin()+7u);
    CHECK(!awe64::creative::candidateReplaySourceMatchedOriginalRamSf2Sample(
        chip,bank,0u,0x200010u,shortSampleTrace,true));
    auto wrongSampleTrace=traced;
    wrongSampleTrace[7].value^=1u;
    CHECK(!awe64::creative::candidateReplaySourceMatchedOriginalRamSf2Sample(
        chip,bank,0u,0x200010u,wrongSampleTrace,true));
    // Changed samples cannot alter previous true chip RAM contents.
    for(unsigned i=0;i<4u;++i)
        CHECK(static_cast<std::uint16_t>(
            core->ram[0x10u+i])==data[i]);
    const auto matched=awe64::creative::
        candidateReplaySourceMatchedOriginalRamSf2Sample(
            chip,bank,0u,0x200010u,traced,true);
    CHECK(matched && matched->original_sample_words==5u &&
          matched->last_word_address==0xffffdfu);
    for(unsigned i=0;i<4u;++i)
        CHECK(static_cast<std::uint16_t>(
            core->ram[0x10u+i])==data[i]);

    // P002 actual source-measured boundary case: the original Creative
    // loader writes the same RAM word more than once. Prove the FINAL
    // recorded physical contents BEFORE any real chip write; keep
    // original source port chronology and reject fake final words.
    Writes originalWithRewrites=traced;
    const std::array<std::uint16_t,1> changedFirst{{0x1234u}};
    addWords(originalWithRewrites,13u,0x200011u,changedFirst);
    const std::array<std::uint16_t,1> restoredOriginal{{data[1]}};
    addWords(originalWithRewrites,15u,0x200011u,restoredOriginal);
    const std::span<const std::uint16_t> expectedFinal(data);
    using awe64::creative::candidateReplayObservedOriginalCreativeFinalRamBank;
    CHECK(!candidateReplayObservedOriginalCreativeFinalRamBank(
        chip,originalWithRewrites,0x200010u,expectedFinal,false,true));
    CHECK(!candidateReplayObservedOriginalCreativeFinalRamBank(
        chip,originalWithRewrites,0x200010u,expectedFinal,true,false));
    CHECK(!candidateReplayObservedOriginalCreativeFinalRamBank(
        chip,originalWithRewrites,0x20000fu,expectedFinal,true,true));
    // The exact FINAL last word matters even if the earlier physical
    // events contain the right value; no normalization is allowed.
    auto invalidFinal=data;
    invalidFinal[3]^=1u;
    CHECK(!candidateReplayObservedOriginalCreativeFinalRamBank(
        chip,originalWithRewrites,0x200010u,invalidFinal,true,true));
    CHECK(static_cast<std::uint16_t>(
        core->ram[0x11u])==data[1]); // no negative-test side effect
    auto sourceWithoutFinalWord=traced;
    sourceWithoutFinalWord.erase(sourceWithoutFinalWord.begin()+7u);
    CHECK(!candidateReplayObservedOriginalCreativeFinalRamBank(
        chip,sourceWithoutFinalWord,0x200010u,expectedFinal,true,true));
    CHECK(!candidateReplayObservedOriginalCreativeFinalRamBank(
        chip,originalWithRewrites,0xffffdeu,expectedFinal,true,true));
    const auto verified=candidateReplayObservedOriginalCreativeFinalRamBank(
        chip,originalWithRewrites,0x200010u,expectedFinal,true,true);
    CHECK(verified &&
          verified->independently_observed_word_base==0x200010u &&
          verified->independently_verified_final_word_count==4u &&
          verified->source_sample_physical_rewrite_events==2u &&
          verified->source_sample_distinct_rewritten_words==1u &&
          verified->original_port_upload.original_sample_words==7u &&
          verified->original_port_upload.exact_port_writes==
              originalWithRewrites.size());
    for(unsigned i=0;i<data.size();++i)
        CHECK(static_cast<std::uint16_t>(
            core->ram[0x10u+i])==data[i]);

    std::cout<<"P002_SOURCE_ORIGINAL_EMU8000_RAM_PORT_REPLAY_PASS"
             <<" source_32bit_word_addresses=TRUE"
             <<" original_SMALW_SMLD_port_order=TRUE"
             <<" unmodified_chip_actual_RAM_contents=TRUE"
             <<" full_preflight_before_hardware_write=TRUE"
             <<" provenance_gate_and_original_28MiB_bounds=TRUE"
             <<" source_guest_trace_in_test=SYNTHETIC_ONLY"
             <<" original_Creative_driver_RAM_upload_verified=NOT_YET"
             <<" exact_source_SF2_pcm_vs_captured_RAM_ports=TRUE"
             <<" source_creative_repeated_RAM_final_word_parity=TRUE"
             <<" negative_invalid_final_RAM_no_chip_mutation=TRUE"
             <<" last_word_corruption_and_wrong_base_no_chip_mutation=TRUE\n";
    return 0;
}
