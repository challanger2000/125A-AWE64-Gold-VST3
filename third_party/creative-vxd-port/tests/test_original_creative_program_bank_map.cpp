#include "OriginalCreativeProgramBankMap.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"VXD_BANK_MAP_FAILURE %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
int main(){
    std::array<OriginalProgramBankSlot,original_program_bank_slots> slots{};
    constexpr std::array<OriginalPresetTableRow,4> slot5{{
        {0,2,4}, {1,7,2}, {1,9,2}, {0xffff,0,0}}};
    constexpr std::array<OriginalPresetTableRow,2> slot130{{
        {1,7,128},{0xffff,0,0}}};
    constexpr std::array<OriginalPresetTableRow,2> slot0{{
        {1,7,0},{0xffff,0,0}}};
    slots[5].rows=slot5;slots[130].rows=slot130;slots[0].rows=slot0;
    OriginalProgramBankMap table{true,slots};
    auto r=select_original_melodic_program(table,2,7);
    CHECK(r.status==ProgramSelectionStatus::Found);
    CHECK(r.device_bank_slot==5u && r.original_preset_row==1u);
    CHECK(r.resolved_bank==2u && r.resolved_program==7u && r.fallback_stage==0u);
    r=select_original_melodic_program(table,3,7);
    CHECK(r.status==ProgramSelectionStatus::Found && r.resolved_bank==128u);
    CHECK(r.device_bank_slot==130u && r.fallback_stage==1u);
    r=select_original_melodic_program(table,0,7);
    CHECK(r.status==ProgramSelectionStatus::Found && r.resolved_bank==128u);
    CHECK(r.device_bank_slot==130u && r.fallback_stage==0u);
    r=select_original_melodic_program(table,128,7);
    CHECK(r.status==ProgramSelectionStatus::Found && r.resolved_bank==128u);
    CHECK(r.device_bank_slot==130u && r.fallback_stage==0u);
    slots[130].rows={};
    r=select_original_melodic_program(table,3,7);
    CHECK(r.status==ProgramSelectionStatus::Found && r.resolved_bank==0u);
    CHECK(r.device_bank_slot==0u && r.fallback_stage==2u);
    r=select_original_melodic_program(table,0,7);
    CHECK(r.status==ProgramSelectionStatus::Found && r.resolved_bank==0u);
    CHECK(r.device_bank_slot==0u && r.fallback_stage==1u);
    r=select_original_melodic_program(table,3,19);
    CHECK(r.status==ProgramSelectionStatus::NoOriginalPreset);
    // A valid original table must be terminated; do not read random memory
    // like a malformed VXD bank pointer or silently invent a sentinel.
    slots[130].rows=std::span<const OriginalPresetTableRow>(slot5.data(),3u);
    CHECK(select_original_melodic_program(table,2,7).status==
          ProgramSelectionStatus::MissingOriginalSource);
    slots[130].rows={};
    table.authentic_driver_records_verified=false;
    CHECK(select_original_melodic_program(table,2,7).status==
          ProgramSelectionStatus::MissingOriginalSource);
    table.authentic_driver_records_verified=true;
    table.slots=std::span<const OriginalProgramBankSlot>(slots.data(),10u);
    CHECK(select_original_melodic_program(table,2,7).status==
          ProgramSelectionStatus::MissingOriginalSource);
    std::puts("P003_ORIGINAL_MELODIC_BANK_PROGRAM_FALLBACK_PASS");
}
