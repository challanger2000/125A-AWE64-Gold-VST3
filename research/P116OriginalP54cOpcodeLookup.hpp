#pragma once
// P116 research-only: EXACT original P54C timing/dependency table dispatch.
// Numeric header is regenerated from SHA-pinned public ORIGINAL 86Box C by
// tools/p116_original_p54c_opcode_table_generator.py at build/test time.
// This is NOT an x86 guest decoder, branch emulator, or executed TSC clock.
// The P005 C++23 pipeline may consume results ONLY after the original
// executed x86 instruction stream has independently supplied all fields.
#include "P116GeneratedP54cTables.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace awe64::research_p116 {
struct OriginalP54cLookupWord {
    std::uint64_t timing_word=0;
    std::uint64_t dependency_word=0;
    std::uint8_t selected_opcode=0;
    bool integer_subset=false;
};

// Match original codegen_timing_pentium_opcode's table-selection switch.
// last_prefix is the ORIGINAL 86Box timing translator's last-prefix state,
// NOT simply a boolean "0F"; op is the opcode supplied to the original
// timing function (for D8..DF that includes the original ModR/M value).
[[nodiscard]] constexpr std::optional<OriginalP54cLookupWord>
originalP54cLookup(std::uint8_t last_prefix,
                   std::uint8_t opcode,
                   std::uint32_t fetchdat) noexcept {
    using namespace data;
    const bool mod3=(fetchdat&0xc0u)==0xc0u;
    const auto group=(fetchdat>>3u)&7u;
    const auto fpuIndex=(static_cast<unsigned>(opcode)>>3u)&7u;

    const auto pair=[&](const auto& memT,const auto& regT,
                       const auto& memD,const auto& regD,
                       const std::size_t i)
        -> std::optional<OriginalP54cLookupWord> {
        if(i >= (mod3?regT.size():memT.size()) ||
           i >= (mod3?regD.size():memD.size()))
            return std::nullopt;
        const std::uint64_t t=mod3?regT[i]:memT[i];
        const std::uint64_t d=mod3?regD[i]:memD[i];
        // P005 integer pipeline currently refuses FXCH/FPU and MMX
        // dependent timing. Preserve original raw table values regardless.
        const auto p=(t>>29u)&7u;
        const bool integer=(t!=0u && p<=3u &&
            (d & (0x3ffull<<24u))==0u &&
            (d & (3ull<<22u))==0u);
        return OriginalP54cLookupWord{t,d,static_cast<std::uint8_t>(i),integer};
    };

    if(last_prefix==0x0fu)
        return pair(opcode_timings_p6_0f,opcode_timings_p6_0f_mod3,
                    opcode_deps_0f,opcode_deps_0f_mod3,opcode);
    // Original last_prefix D8..DF are x87 families. Retain them for
    // source-differential proof but NEVER advertise them as integer.
    switch(last_prefix) {
        case 0xd8u:
            return pair(opcode_timings_p6_d8,opcode_timings_p6_d8_mod3,
                        opcode_deps_d8,opcode_deps_d8_mod3,fpuIndex);
        case 0xd9u:
            return pair(opcode_timings_p6_d9,opcode_timings_p6_d9_mod3,
                        opcode_deps_d9,opcode_deps_d9_mod3,
                        mod3?(opcode&0x3fu):fpuIndex);
        case 0xdau:
            return pair(opcode_timings_p6_da,opcode_timings_p6_da_mod3,
                        opcode_deps_da,opcode_deps_da_mod3,fpuIndex);
        case 0xdbu:
            return pair(opcode_timings_p6_db,opcode_timings_p6_db_mod3,
                        opcode_deps_db,opcode_deps_db_mod3,
                        mod3?(opcode&0x3fu):fpuIndex);
        case 0xdcu:
            return pair(opcode_timings_p6_dc,opcode_timings_p6_dc_mod3,
                        opcode_deps_dc,opcode_deps_dc_mod3,fpuIndex);
        case 0xddu:
            return pair(opcode_timings_p6_dd,opcode_timings_p6_dd_mod3,
                        opcode_deps_dd,opcode_deps_dd_mod3,fpuIndex);
        case 0xdeu:
            return pair(opcode_timings_p6_de,opcode_timings_p6_de_mod3,
                        opcode_deps_de,opcode_deps_de_mod3,fpuIndex);
        case 0xdfu:
            return pair(opcode_timings_p6_df,opcode_timings_p6_df_mod3,
                        opcode_deps_df,opcode_deps_df_mod3,fpuIndex);
        default:break;
    }
    switch(opcode) {
        case 0x80u: case 0x82u: case 0x83u:
            return pair(opcode_timings_p6_8x,opcode_timings_p6_8x_mod3,
                        opcode_deps_8x,opcode_deps_8x_mod3,group);
        case 0x81u:
            return pair(opcode_timings_p6_81,opcode_timings_p6_81_mod3,
                        opcode_deps_81,opcode_deps_81_mod3,group);
        case 0xc0u: case 0xc1u: case 0xd0u: case 0xd1u:
            return pair(opcode_timings_p6_shift,opcode_timings_p6_shift_mod3,
                        opcode_deps_shift,opcode_deps_shift_mod3,group);
        case 0xd2u: case 0xd3u:
            return pair(opcode_timings_p6_shift,opcode_timings_p6_shift_mod3,
                        opcode_deps_shift_cl,opcode_deps_shift_cl_mod3,group);
        case 0xf6u:
            return pair(opcode_timings_p6_f6,opcode_timings_p6_f6_mod3,
                        opcode_deps_f6,opcode_deps_f6_mod3,group);
        case 0xf7u:
            return pair(opcode_timings_p6_f7,opcode_timings_p6_f7_mod3,
                        opcode_deps_f7,opcode_deps_f7_mod3,group);
        case 0xffu:
            return pair(opcode_timings_p6_ff,opcode_timings_p6_ff_mod3,
                        opcode_deps_ff,opcode_deps_ff_mod3,group);
        default:
            return pair(opcode_timings_p6,opcode_timings_p6_mod3,
                        opcode_deps,opcode_deps_mod3,opcode);
    }
}
} // namespace awe64::research_p116
