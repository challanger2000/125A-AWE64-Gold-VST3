#include "OriginalCreativeProgramBankMap.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"VXD_DRUM_BANK_FAIL %d %s\n",__LINE__,#x);std::exit(1);}}while(false)
int main(){
    std::array<OriginalProgramBankSlot,original_program_bank_slots> banks{};
    constexpr std::array<OriginalPresetTableRow,4> b12{{
        {1u,131u,12u},{1u,133u,12u},{1u,128u,12u},{0xffffu,0u,0u}}};
    constexpr std::array<OriginalPresetTableRow,2> b128{{
        {1u,128u,128u},{0xffffu,0u,0u}}};
    constexpr std::array<OriginalPresetTableRow,2> b0{{
        {1u,135u,0u},{0xffffu,0u,0u}}};
    banks[12].rows=b12;banks[128].rows=b128;banks[0].rows=b0;
    OriginalProgramBankMap map{true,banks};
    auto s=select_original_percussion_program(map,12u,5u);
    CHECK(s.status==ProgramSelectionStatus::Found);
    CHECK(s.device_bank_slot==12u && s.original_preset_row==1u);
    CHECK(s.resolved_bank==12u && s.resolved_program==133u);
    s=select_original_percussion_program(map,12u,6u);
    CHECK(s.status==ProgramSelectionStatus::Found && s.resolved_program==133u);
    CHECK(s.device_bank_slot==12u); // closest earlier original drum preset
    s=select_original_percussion_program(map,0u,7u);
    CHECK(s.status==ProgramSelectionStatus::Found && s.resolved_bank==0u);
    CHECK(s.resolved_program==135u && s.device_bank_slot==0u);
    banks[0].rows={};
    s=select_original_percussion_program(map,0u,7u);
    CHECK(s.status==ProgramSelectionStatus::Found && s.resolved_bank==128u);
    CHECK(s.resolved_program==128u);
    banks[12].rows={};
    s=select_original_percussion_program(map,12u,5u);
    CHECK(s.status==ProgramSelectionStatus::Found && s.resolved_bank==128u);
    CHECK(s.resolved_program==128u);
    banks[128].rows={};
    s=select_original_percussion_program(map,12u,5u);
    CHECK(s.status==ProgramSelectionStatus::NoOriginalPreset);
    banks[12].rows=std::span<const OriginalPresetTableRow>(b12.data(),3u);
    CHECK(select_original_percussion_program(map,12u,5u).status==
          ProgramSelectionStatus::MissingOriginalSource);
    map.authentic_driver_records_verified=false;
    CHECK(select_original_percussion_program(map,12u,5u).status==
          ProgramSelectionStatus::MissingOriginalSource);
    std::puts("P003_ORIGINAL_DRUM_BANK_BEST_FALLBACK_PASS");
}
