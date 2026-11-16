#include "OriginalCreativeProgramBankMap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace awe64::original_vxd {
OriginalProgramSelection select_original_melodic_program(
    OriginalProgramBankMap const& b,const std::uint8_t bank,
    const std::uint8_t program) noexcept {
    OriginalProgramSelection r{};
    if(!b.authentic_driver_records_verified ||
       b.slots.size()!=original_program_bank_slots || program>=128u)
        return r;
    // In the original LE LCOD+0x6E71, 288 device bank-slot pointers
    // may point to 26-byte rows. No fabricated null sentinel or endless
    // scan: authenticate the end marker before starting.
    for(auto const& slot:b.slots) {
        if(slot.rows.empty())continue; // genuine null table entry
        bool sentinel=false;
        if(slot.rows.size()>65536u)return r;
        for(auto const& row:slot.rows){
            if(row.original_source_marker==0xffffu) {
                sentinel=true;break;
            }
        }
        if(!sentinel)return r;
    }
    struct Stage {std::uint16_t start;std::uint8_t expected_bank;};
    // Source x86: for selected bank=0 begin at slot128 looking for 128;
    // for a nonzero bank scan slot0..287 with that bank ID. If no match,
    // source fallback tests 128, then 0. See +0x6E8C..+0x6EF3.
    std::array<Stage,3> stages{};
    unsigned count=0u;
    if(bank==0u)stages[count++]={128u,128u};
    else stages[count++]={0u,bank};
    if(bank!=128u && bank!=0u)stages[count++]={128u,128u};
    stages[count++]={0u,0u};
    for(unsigned stage=0u;stage<count;++stage) {
        const auto& s=stages[stage];
        for(std::uint32_t slot=s.start;slot<original_program_bank_slots;++slot) {
            std::size_t ordinal=0u;
            for(auto const& row:b.slots[slot].rows) {
                if(row.original_source_marker==0xffffu)break;
                if(row.original_source_marker!=0u &&
                   row.original_bank_byte==s.expected_bank &&
                   row.original_program_byte==program) {
                    r.status=ProgramSelectionStatus::Found;
                    r.device_bank_slot=static_cast<std::uint16_t>(slot);
                    r.original_preset_row=static_cast<std::uint16_t>(ordinal);
                    r.resolved_program=program;
                    r.resolved_bank=s.expected_bank;
                    r.fallback_stage=static_cast<std::uint8_t>(stage);
                    return r;
                }
                ++ordinal;
            }
        }
    }
    r.status=ProgramSelectionStatus::NoOriginalPreset;
    return r;
}
OriginalProgramSelection select_original_percussion_program(
    OriginalProgramBankMap const& b,const std::uint8_t bank,
    const std::uint8_t program) noexcept {
    OriginalProgramSelection result{};
    if(!b.authentic_driver_records_verified ||
       b.slots.size()!=original_program_bank_slots || program>=128u)
        return result;
    for(const auto& slot:b.slots) {
        if(slot.rows.empty())continue;
        bool terminated=false;
        if(slot.rows.size()>65536u)return result;
        for(const auto& row:slot.rows) {
            if(row.original_source_marker==0xffffu) {terminated=true;break;}
        }
        if(!terminated)return result;
    }
    std::uint32_t requested=static_cast<std::uint32_t>(program)|0x80u;
    std::uint32_t desired=bank==0u?128u:bank;
    std::uint32_t current_slot=desired;
    std::uint32_t lower_bound=128u;
    bool no_partial_candidate=true;
    unsigned stage=0u;
    // LCOD+0x6D8C..0x6E63. All loops are bounded by the 288-slot
    // original bank-directory capacity; no malloc or table synthesis.
    for(unsigned fallback=0u;fallback<3u;++fallback) {
        while(current_slot<original_program_bank_slots) {
            const auto& slot=b.slots[current_slot];
            std::uint32_t row_index=0u;
            for(const auto& row:slot.rows) {
                if(row.original_source_marker==0xffffu)break;
                const std::uint32_t patch=row.original_program_byte;
                if(row.original_source_marker!=0u &&
                   row.original_bank_byte==desired &&
                   patch>=lower_bound && patch<=requested) {
                    result.status=ProgramSelectionStatus::Found;
                    result.device_bank_slot=static_cast<std::uint16_t>(current_slot);
                    result.original_preset_row=static_cast<std::uint16_t>(row_index);
                    result.resolved_program=static_cast<std::uint16_t>(patch);
                    result.resolved_bank=static_cast<std::uint8_t>(desired);
                    result.fallback_stage=static_cast<std::uint8_t>(stage);
                    lower_bound=patch;
                    if(patch==requested)return result;
                    no_partial_candidate=false;
                }
                ++row_index;
            }
            // Source +0x6E0B jumps straight from any slot <=128
            // to 129. In particular, the original drum search is NOT
            // a generic 'scan all 0..287' table traversal.
            current_slot=current_slot<=128u?129u:current_slot+1u;
        }
        if(!no_partial_candidate && (desired==0u || bank!=0u))
            return result;
        if(desired==0u)break;
        if(desired==128u) {
            desired=0u;current_slot=0u;
        } else {
            desired=128u;current_slot=128u;requested=128u;
        }
        ++stage;
    }
    // The original returns failure when no matching bank records exist.
    if(result.status!=ProgramSelectionStatus::Found)
        result.status=ProgramSelectionStatus::NoOriginalPreset;
    return result;
}

} // namespace awe64::original_vxd
