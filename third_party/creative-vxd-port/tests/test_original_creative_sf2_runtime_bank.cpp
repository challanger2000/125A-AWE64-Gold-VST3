#include "OriginalSf2RuntimeFixture.hpp"
#include "OriginalCreativeMidiRamRuntime.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
using namespace awe64::original_vxd;
using namespace awe64::original_vxd::test;
namespace {bool measure=false,failAllocations=false;std::size_t allocations=0;}
void* operator new(std::size_t size) {if(failAllocations)throw std::bad_alloc();if(measure)++allocations;auto* p=std::malloc(size?size:1u);if(!p)throw std::bad_alloc();return p;}
void* operator new[](std::size_t size) {return ::operator new(size);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"RUNTIME_BANK_FAIL %d %s\n",__LINE__,#x);std::abort();}}while(false)
int main() {
    RuntimeFixture fixture;
    OriginalCreativeSf2RuntimeBank bank;
    constexpr std::size_t budget=128u*1024u;
    CHECK(bank.prepareNonRealtime(fixture.slots,fixture.files,fixture.bindings,
        fixture.resources,budget,false)==Sf2RuntimeBankStatus::Unverified);
    CHECK(bank.prepareNonRealtime(fixture.slots,fixture.files,fixture.bindings,
        fixture.resources,1u,true)==Sf2RuntimeBankStatus::BudgetExceeded);
    auto duplicate=fixture.bindings;duplicate[1]=duplicate[0];
    CHECK(bank.prepareNonRealtime(fixture.slots,fixture.files,duplicate,
        fixture.resources,budget,true)==Sf2RuntimeBankStatus::Malformed);
    auto badBinding=fixture.bindings;badBinding[0].source_program=9u;
    CHECK(bank.prepareNonRealtime(fixture.slots,fixture.files,badBinding,
        fixture.resources,budget,true)==Sf2RuntimeBankStatus::Malformed);
    auto overlapping=fixture.files;overlapping[1].tables.ram_base_word=0x20000fu;
    CHECK(bank.prepareNonRealtime(fixture.slots,overlapping,fixture.bindings,
        fixture.resources,budget,true)==Sf2RuntimeBankStatus::Malformed);
    const auto pool=source_sf2_raw_sample_pool(fixture.bytes);
    CHECK(pool);
    const auto words=static_cast<std::uint32_t>(pool->size()/2u);
    overlapping[1].tables.ram_base_word=overlapping[0].tables.ram_base_word+words;
    CHECK(bank.prepareNonRealtime(fixture.slots,overlapping,fixture.bindings,
        fixture.resources,budget,true)==Sf2RuntimeBankStatus::Malformed);
    overlapping[1].tables.ram_base_word+=16u;
    OriginalCreativeSf2RuntimeBank adjacent;
    CHECK(adjacent.prepareNonRealtime(fixture.slots,overlapping,fixture.bindings,
        fixture.resources,budget,true)==Sf2RuntimeBankStatus::Ok);
    CHECK(!bank.ready());
    failAllocations=true;
    const auto failure=bank.prepareNonRealtime(fixture.slots,fixture.files,fixture.bindings,
        fixture.resources,budget,true);
    failAllocations=false;
    CHECK(failure==Sf2RuntimeBankStatus::AllocationFailed && !bank.ready());
    CHECK(bank.prepareNonRealtime(fixture.slots,fixture.files,fixture.bindings,
        fixture.resources,budget,true)==Sf2RuntimeBankStatus::Ok);
    CHECK(bank.retainedBytes()<=budget && bank.fileCount()==2u && bank.presetBindingCount()==5u);
    const auto retained=bank.retainedBytes();
    OriginalCreativeSf2RuntimeBank tooTight;
    CHECK(tooTight.prepareNonRealtime(fixture.slots,fixture.files,fixture.bindings,
        fixture.resources,retained-1u,true)==Sf2RuntimeBankStatus::BudgetExceeded);
    CHECK(!tooTight.ready());
    const auto legacyMerged=select_sf2_raw_layers(fixture.bytes,0u,7u,60u,100u);
    CHECK(legacyMerged.count==4u);
    CHECK(select_sf2_raw_layers_by_preset(fixture.bytes,0u,60u,100u).count==2u);
    CHECK(select_sf2_raw_layers_by_preset(fixture.bytes,1u,60u,100u).count==2u);
    fixture.bytes.assign(12u,0u);fixture.row5[0].original_program_byte=99u;
    fixture.resources.velocity.fill(255u);fixture.resources.volume_positive16.fill(0u);
    OriginalCreativeMidiRamRuntime runtime;
    RuntimeChip chip;
    CHECK(runtime.bind_chip(chip));runtime.midi=runtimeState();
    runtime.midi.volume_source=bank.controllerResources();
    auto source=bank.source();
    auto event=[&](std::uint8_t status,std::uint8_t a,std::uint8_t b=0u) {
        return runtime.midi_event(chip,status,a,b,source);
    };
    CHECK(event(0xb0u,0u,1u)==MidiResult::Applied);
    CHECK(event(0xc0u,0u)==MidiResult::Applied);
    CHECK(runtime.midi.channel[0].original_selected_preset_row==1u);
    std::array<SourceVoice,max_source_note_layers> voices{};std::uint8_t count=0u;
    CHECK(source.resolve(source.opaque,runtime.midi.channel[0],60u,100u,voices,count));
    CHECK(count==2u && voices[0].descriptor[0x8cu]==1u);
    CHECK(voices[0].descriptor[0x90u]==0x21u && voices[0].descriptor[0x93u]==0x87u);
    OriginalVoiceProgram plan{};auto stale=voices[0];
    CHECK(source.compile(source.opaque,stale,0u,60u,plan));
    CHECK(source.resolve(source.opaque,runtime.midi.channel[0],61u,100u,voices,count));
    CHECK(!source.compile(source.opaque,stale,0u,60u,plan));
    auto unknown=runtime.midi.channel[0];unknown.original_fine_tune_verified=false;
    CHECK(!source.resolve(source.opaque,unknown,60u,100u,voices,count));
    CHECK(!source.compile(source.opaque,stale,0u,60u,plan));
    // An entire two-bank, two-program key/velocity space, without storing
    // even one prepared note directory. Count heap allocations in callbacks.
    std::size_t notes=0u;
    measure=true;
    for(const auto logicalBank:{1u,2u}) {
        CHECK(event(0xb0u,0u,static_cast<std::uint8_t>(logicalBank))==MidiResult::Applied);
        for(const auto program:{0u,7u}) {
            CHECK(event(0xc0u,static_cast<std::uint8_t>(program))==MidiResult::Applied);
            for(unsigned key=0u;key<128u;++key)for(unsigned velocity=1u;velocity<128u;++velocity) {
                CHECK(event(0x90u,static_cast<std::uint8_t>(key),static_cast<std::uint8_t>(velocity))==MidiResult::Applied);
                CHECK(event(0x80u,static_cast<std::uint8_t>(key))==MidiResult::Applied);
                ++notes;
            }
        }
    }
    measure=false;
    CHECK(notes==65024u && allocations==0u && bank.retainedBytes()==retained);
    // CC0 alone does not replace the source row latched by the preceding C0.
    CHECK(event(0xb0u,0u,1u)==MidiResult::Applied);
    CHECK(event(0x90u,60u,127u)==MidiResult::Applied);
    CHECK(runtime.midi.channel[0].original_selected_bank==2u);
    CHECK(event(0x80u,60u)==MidiResult::Applied);
    // Original channel-10 program lookup, with logical bank128/program128
    // bound explicitly to source PHDR row1 (not a fabricated drum preset).
    CHECK(event(0xb9u,0u,0u)==MidiResult::Applied);
    CHECK(event(0xc9u,0u)==MidiResult::Applied);
    CHECK(runtime.midi.channel[9].original_selected_program==128u);
    CHECK(event(0x99u,60u,127u)==MidiResult::Applied);
    CHECK(event(0x89u,60u)==MidiResult::Applied);
    CHECK(event(0xc0u,99u)==MidiResult::MissingSource);
    const auto before=chip.writes;
    CHECK(event(0x90u,60u,127u)==MidiResult::MissingSource);
    CHECK(chip.writes==before && !runtime.midi.fault);
    CHECK(bank.resetNonRealtime());
    CHECK(!source.compile(source.opaque,stale,0u,60u,plan));
    RuntimeFixture again;
    CHECK(bank.prepareNonRealtime(again.slots,again.files,again.bindings,
        again.resources,budget,true)==Sf2RuntimeBankStatus::Ok);
    CHECK(!source.compile(source.opaque,stale,0u,60u,plan));
    // Logical bank128 does not imply percussion: LCOD tests the selected
    // program WORD for its velocity/filter branch. Preserve source +59FC.
    {
        RuntimeFixture melodic;
        std::array<OriginalPresetTableRow,2> rows{{{1u,7u,128u,33u},{0xffffu,0u,0u,0u}}};
        melodic.slots[128].rows=rows;
        auto bindings=std::vector<OriginalSf2RuntimePresetBinding>(melodic.bindings.begin(),melodic.bindings.end());
        bindings.push_back({true,128u,0u,0u,0u,0u,7u,0x1234u});
        melodic.files[0].tables.instrument_template[34u]=100;
        OriginalCreativeSf2RuntimeBank melodicBank;
        CHECK(melodicBank.prepareNonRealtime(melodic.slots,melodic.files,bindings,
            melodic.resources,budget,true)==Sf2RuntimeBankStatus::Ok);
        auto state=runtimeState();auto s=melodicBank.source();
        CHECK(dispatch_midi(state,0xc0u,7u,0u,{},s)==MidiResult::Applied);
        CHECK(state.channel[0].original_selected_bank==128u);
        CHECK(s.resolve(s.opaque,state.channel[0],60u,1u,voices,count));
        CHECK(voices[0].note_start_descriptor[16u]==99u);
    }
    // Unknown SF2 modulators remain unsupported, not ignored/faked.
    {
        RuntimeFixture modulated;
        modulated.bytes=runtimeSf2(true);
        for(auto& file:modulated.files)file.bytes=modulated.bytes;
        OriginalCreativeSf2RuntimeBank unsupported;
        CHECK(unsupported.prepareNonRealtime(modulated.slots,modulated.files,modulated.bindings,
            modulated.resources,budget,true)==Sf2RuntimeBankStatus::Ok);
        OriginalCreativeMidiRamRuntime r;RuntimeChip b;
        CHECK(r.bind_chip(b));r.midi=runtimeState();const auto s=unsupported.source();
        CHECK(r.midi_event(b,0xb0u,0u,1u,s)==MidiResult::Applied);
        CHECK(r.midi_event(b,0xc0u,0u,0u,s)==MidiResult::Applied);
        const auto before=b.writes;
        CHECK(r.midi_event(b,0x90u,60u,100u,s)==MidiResult::MissingSource);
        CHECK(b.writes==before && unsupported.lastNoteStatus()==Sf2BuildStatus::Unsupported);
    }
    std::printf("SYNTHETIC_RUNTIME_SF2_BANK_PASS notes=%zu retained_bytes=%zu heap_allocations=%zu original_win95_parity=NOT_PROVEN\n",notes,retained,allocations);
}
