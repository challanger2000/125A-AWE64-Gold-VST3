// P005 regression for authentic guest event ORDER, not original CPU timing
// or private original descriptor parity. Source values below are SYNTHETIC.
#include "P005OriginalCreativeHostAdapter.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
using namespace awe64::original_vxd;
using awe64::vst3::P005OriginalCreativeHostAdapter;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"P005_PREBANK2_FAIL:%d %s\n",__LINE__,#x);return 1;}}while(false)
namespace {
struct Chip {
    bool initialized=true;
    std::uint64_t writes=0,reads=0;
    [[nodiscard]] bool ready() const noexcept{return initialized;}
    void writePort(std::uint16_t,std::uint16_t) noexcept {++writes;}
    [[nodiscard]] std::uint16_t readPort(std::uint16_t) noexcept {++reads;return 0u;}
};
void word(Descriptor& d,unsigned at,std::uint16_t x) noexcept {
    d[at]=static_cast<std::uint8_t>(x);d[at+1u]=static_cast<std::uint8_t>(x>>8u);
}
void dword(Descriptor& d,unsigned at,std::uint32_t x) noexcept {
    for(unsigned i=0;i<4u;++i)d[at+i]=static_cast<std::uint8_t>(x>>(8u*i));
}
VerifiedSf2Zone sourceFixture() noexcept {
    VerifiedSf2Zone zone{};
    zone.original_zone_verified=true;zone.bank=1u;zone.program=0u;
    zone.original_preset_identity_verified=true;
    zone.original_bank_slot=5u;zone.original_preset_row=0u;
    zone.key_low=60u;zone.key_high=62u;
    zone.voice.source_verified=true;
    zone.voice.original_pitch_base_verified=true;
    zone.voice.original_pitch_base=0xe000u;
    zone.voice.note_start_descriptor_verified=true;
    auto& d=zone.voice.note_start_descriptor;
    word(d,0x8cu,1u);word(d,54u*2u,1u);word(d,8u*2u,180u);
    word(d,9u*2u,7u);word(d,19u*2u,20u);word(d,17u*2u,64u);
    word(d,18u*2u,128u);word(d,26u*2u,128u);word(d,27u*2u,127u);
    word(d,28u*2u,4u);word(d,29u*2u,127u);word(d,30u*2u,12u);
    word(d,33u*2u,0x8000u);word(d,34u*2u,128u);word(d,35u*2u,127u);
    word(d,36u*2u,63u);word(d,37u*2u,5u);word(d,38u*2u,52u);
    word(d,21u*2u,0x8000u);word(d,23u*2u,0x8000u);
    word(d,22u*2u,205u);word(d,24u*2u,205u);
    word(d,56u*2u,100u);word(d,58u*2u,0xffffu);d[0x8au]=60u;
    dword(d,0x86u,0xe00000u);
    dword(d,0x76u,0x200100u);dword(d,0x7au,0x200200u);
    dword(d,0x7eu,0x200120u);dword(d,0x82u,0x2001e0u);
    zone.voice.descriptor=d;word(zone.voice.descriptor,0x6au,20u);
    zone.original_state.original_descriptor_verified=true;
    zone.original_state.original_device_state_verified=true;
    zone.original_state.original_sample_placement_verified=true;
    zone.original_state.original_pitch_target=0x8000u;
    zone.original_state.original_ram_sample=true;
    return zone;
}
}
int main(){
    Chip chip{};
    P005OriginalCreativeHostAdapter<Chip> host{};
    std::array<VerifiedSf2Zone,1> zones{sourceFixture()};
    std::array<OriginalProgramBankSlot,original_program_bank_slots> slots{};
    const std::array<OriginalPresetTableRow,2> rows{{
        {1u,0u,1u},{0xffffu,0u,0u}}};
    slots[5].rows=rows;
    VerifiedSf2Bank bank{zones};
    bank.original_program_map={true,slots};
    CHECK(host.activate(chip,bank,true));
    auto* shared=host.sharedRuntime();
    CHECK(shared && shared->scratch.device==&shared->midi.device);
    // First RAM-bank upload finished BEFORE the Win95 C4/D4 sequence.
    // C4 WinMM: bankMSB1, program0, FX=0, NoteOn and NoteOff.
    CHECK(host.controller(0u,0u,1u)==MidiResult::Applied);
    CHECK(host.program(0u,0u)==MidiResult::Applied);
    CHECK(host.controller(0u,91u,0u)==MidiResult::Applied);
    CHECK(host.controller(0u,93u,0u)==MidiResult::Applied);
    CHECK(shared->midi.channel[0].original_program_selection_valid);
    const auto before=chip.writes;
    CHECK(host.noteOn(0u,60u,100u)==MidiResult::Applied);
    CHECK(chip.writes>before);
    CHECK(host.noteOff(0u,60u)==MidiResult::Applied);
    // Original AWEMAN32 D4 path shares the same original device.
    CHECK(host.controller(0u,0u,1u)==MidiResult::Applied);
    CHECK(host.program(0u,0u)==MidiResult::Applied);
    CHECK(host.controller(0u,91u,0u)==MidiResult::Applied);
    CHECK(host.controller(0u,93u,0u)==MidiResult::Applied);
    CHECK(host.noteOn(0u,62u,100u)==MidiResult::Applied);
    CHECK(host.noteOff(0u,62u)==MidiResult::Applied);
    CHECK(host.active() && shared==host.sharedRuntime());
    CHECK(shared->scratch.device==&shared->midi.device);
    CHECK(!shared->scratch.active && !shared->scratch.faulted);
    CHECK(!shared->midi.fault);
    CHECK(chip.writes>before && chip.reads>0u);
    // Bank2 begins here. 4176 later stress messages NEVER preceded it.
    const auto previousCount=shared->midi.device.mode_count;
    const auto previousKey=shared->midi.device.voices[0].key_marker;
    const std::array<std::uint8_t,4> invalidBank{{0,1,2,3}};
    OriginalCreativeRamBankCursor bank2{true,false,0x23fb49u};
    CHECK(shared->upload_bank(chip,invalidBank,bank2).status==
          OriginalSf2UploadStatus::MalformedBank);
    CHECK(shared->midi.device.mode_count==previousCount);
    CHECK(shared->midi.device.voices[0].key_marker==previousKey);
    CHECK(shared->scratch.device==&shared->midi.device);
    std::puts("P005_C4_D4_PREBANK2_ONE_DEVICE_NO_STRESS_PASS synthetic_only=TRUE native_parity=NOT_PROVEN");
    return 0;
}
