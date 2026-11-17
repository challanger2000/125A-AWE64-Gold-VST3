// Synthetic ROM/SF2/driver state -> original-port RAM loader -> on-demand
// original reader/compiler/MIDI allocator -> unchanged EMU8000 -> PCM.
// Functional check only, NOT original Creative/Win95 PCM equivalence.
#include "Chip.h"
#include "OriginalCreativeMidiRamRuntime.hpp"
#include "OriginalSf2RuntimeFixture.hpp"
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
using namespace awe64::original_vxd;
using namespace awe64::original_vxd::test;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"RUNTIME_REAL_CHIP_FAIL %d %s\n",__LINE__,#x);return 1;}}while(false)
int run(bool invert,std::array<std::int32_t,4096>& pcm) {
    const auto rom=std::filesystem::temp_directory_path()/"awe64-p005-runtime-synthetic-rom.raw";
    {
        std::ofstream out(rom,std::ios::binary|std::ios::trunc);
        const std::array<char,4096> zero{};
        for(unsigned i=0;i<256u;++i)out.write(zero.data(),zero.size());
        CHECK(out.good());
    }
    awe64::Chip chip;std::string error;
    const bool opened=chip.open(rom,error);std::error_code ec;std::filesystem::remove(rom,ec);
    CHECK(opened);
    RuntimeFixture fixture;
    if(invert) {
        const auto pool=source_sf2_raw_sample_pool(fixture.bytes);
        CHECK(pool);
        const auto offset=static_cast<std::size_t>(pool->data()-fixture.bytes.data());
        for(std::size_t i=0;i<pool->size();i+=2u) {
            const auto word=static_cast<std::uint16_t>(fixture.bytes[offset+i]|
                (static_cast<unsigned>(fixture.bytes[offset+i+1u])<<8u));
            const auto opposite=static_cast<std::uint16_t>(0u-word);
            fixture.bytes[offset+i]=static_cast<std::uint8_t>(opposite);
            fixture.bytes[offset+i+1u]=static_cast<std::uint8_t>(opposite>>8u);
        }
    }
    OriginalCreativeMidiRamRuntime runtime;CHECK(runtime.bind_chip(chip));
    runtime.midi=runtimeState();
    OriginalCreativeRamBankCursor cursor{true,false,0x200000u};
    const auto uploaded=runtime.upload_bank(chip,fixture.bytes,cursor);
    CHECK(uploaded.status==OriginalSf2UploadStatus::Ok);
    CHECK(bind_original_creative_uploaded_ram_bank(fixture.files[0].tables,uploaded));
    fixture.slots[6].rows={};
    const std::array<OriginalSf2RuntimePresetBinding,3> bindings{
        fixture.bindings[0],fixture.bindings[1],fixture.bindings[4]};
    OriginalCreativeSf2RuntimeBank bank;
    CHECK(bank.prepareNonRealtime(fixture.slots,std::span(fixture.files).first(1u),bindings,
        fixture.resources,128u*1024u,true)==Sf2RuntimeBankStatus::Ok);
    runtime.midi.volume_source=bank.controllerResources();
    const auto source=bank.source();
    auto midi=[&](std::uint8_t status,std::uint8_t a,std::uint8_t b=0u) {
        return runtime.midi_event(chip,status,a,b,source);
    };
    CHECK(midi(0xb0u,0u,1u)==MidiResult::Applied);
    CHECK(midi(0xc0u,0u)==MidiResult::Applied);
    CHECK(midi(0xb0u,7u,127u)==MidiResult::Applied);
    CHECK(midi(0xb0u,11u,127u)==MidiResult::Applied);
    CHECK(chip.armPortWriteTrace(4096u));
    std::array<std::int32_t,256> silence{};
    chip.render(silence.data(),128u);
    for(const auto value:silence)CHECK(value==0);
    runtime.midi.channel[0].original_fine_tune_verified=false;
    const auto noSourceWrites=chip.portWriteTrace().size();
    CHECK(midi(0x90u,60u,127u)==MidiResult::MissingSource);
    CHECK(chip.portWriteTrace().size()==noSourceWrites);
    runtime.midi.channel[0].original_fine_tune_verified=true;
    CHECK(midi(0x90u,60u,127u)==MidiResult::Applied);
    CHECK(runtime.scratch.device==&runtime.midi.device && runtime.scratch.cycles_completed!=0u);
    chip.render(pcm.data(),2048u);
    std::size_t nonzero=0u;
    for(const auto sample:pcm)if(sample!=0)++nonzero;
    CHECK(nonzero!=0u && !chip.portWriteTraceOverflow());
    CHECK(midi(0x80u,60u)==MidiResult::Applied);
    CHECK(midi(0xc0u,7u)==MidiResult::Applied);
    CHECK(midi(0x90u,62u,100u)==MidiResult::Applied);
    CHECK(midi(0x80u,62u)==MidiResult::Applied);
    std::printf("SYNTHETIC_RUNTIME_REAL_EMU8000_PCM_PASS nonzero_samples=%zu upload_cycles=%u original_win95_parity=NOT_PROVEN\n",
        nonzero,runtime.scratch.cycles_completed);
    return 0;
}
int main() {
    std::array<std::int32_t,4096> first{},second{};
    CHECK(run(false,first)==0);
    CHECK(run(true,second)==0);
    CHECK(first!=second); // the uploaded samples, not a ROM fallback, drive PCM
    std::puts("SYNTHETIC_RUNTIME_REAL_CHIP_CHANGED_SAMPLES_CHANGE_PCM_PASS");
}
