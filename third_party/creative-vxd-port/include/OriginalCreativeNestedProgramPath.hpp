#pragma once
// P005 SOURCE-ONLY index traversal of the owner-observed Creative GM bank.
// No Creative bytes, DSP, timing, SF2 synthesis, or production enablement.
// The caller authenticates the private original Win95 device/guest regions.
// Opcode 41/53 is a structural association, NOT a proof of VXD semantics.
#include "OriginalCreativeProgramBankMap.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace awe64::original_vxd {
struct OriginalNestedProgramTables {
    std::span<const OriginalPresetTableRow> programs{}; // sentinel included
    std::span<const std::uint8_t> preset_bag_offsets{}; // LE u32
    std::span<const std::uint8_t> preset_generators{}; // opcode + LE u16
    std::span<const std::uint8_t> instrument_bag_offsets{}; // LE u16
    std::span<const std::uint8_t> instrument_generator_offsets{}; // LE u32
    std::span<const std::uint8_t> instrument_generators{}; // opcode + LE u16
    bool independently_authenticated=false;
};
enum class NestedPathResult : std::uint8_t {
    Ok, Unverified, Malformed, MissingReference, OutputTooSmall
};
struct NestedPathCounts {
    std::size_t groups=0;
    std::size_t instrument_bags=0;
    std::size_t sample_id_references=0;
};
// Non-RT diagnostic only. Never silently truncates or deduplicates sample IDs.
// A two-pass walk protects the output from partial writes on any failure.
// This deliberately DOES NOT follow incomplete sample PCM tables.
[[nodiscard]] inline NestedPathResult original_nested_program_sample_ids(
    const OriginalNestedProgramTables& t,
    std::size_t row_index,
    std::span<std::uint16_t> samples,
    NestedPathCounts& counts) noexcept {
    counts={};
    if(!t.independently_authenticated)return NestedPathResult::Unverified;
    if(t.programs.size()<2u ||
       t.preset_bag_offsets.size()%4u!=0u ||
       t.instrument_bag_offsets.size()%2u!=0u ||
       t.instrument_generator_offsets.size()%4u!=0u ||
       t.preset_generators.size()%3u!=0u ||
       t.instrument_generators.size()%3u!=0u ||
       t.preset_bag_offsets.size()<8u ||
       t.instrument_bag_offsets.size()<4u ||
       t.instrument_generator_offsets.size()<8u ||
       row_index+1u>=t.programs.size() ||
       t.programs.back().original_source_marker!=0xffffu ||
       t.programs[row_index].original_source_marker==0xffffu)
        return NestedPathResult::Malformed;
    auto u16=[](std::span<const std::uint8_t> s,std::size_t i) noexcept {
        return static_cast<std::uint32_t>(s[i] |
            (static_cast<std::uint32_t>(s[i+1u])<<8u));
    };
    auto u32=[&](std::span<const std::uint8_t> s,std::size_t n) noexcept {
        const auto p=n*4u;
        return u16(s,p) | (u16(s,p+2u)<<16u);
    };
    const auto n_preset=t.preset_bag_offsets.size()/4u;
    const auto n_inst=t.instrument_bag_offsets.size()/2u;
    const auto n_ibag=t.instrument_generator_offsets.size()/4u;
    const auto preset_triples=t.preset_generators.size()/3u;
    const auto inst_triples=t.instrument_generators.size()/3u;
    // Validate the three complete original index arrays and sentinels;
    // do not use guessed group/bag lengths or PHDR ordering.
    if(u32(t.preset_bag_offsets,0)!=0u ||
       u16(t.instrument_bag_offsets,0)!=0u ||
       u32(t.instrument_generator_offsets,0)!=0u ||
       u32(t.preset_bag_offsets,n_preset-1u)!=preset_triples ||
       u16(t.instrument_bag_offsets,(n_inst-1u)*2u)!=n_ibag-1u ||
       u32(t.instrument_generator_offsets,n_ibag-1u)!=inst_triples)
        return NestedPathResult::Malformed;
    for(std::size_t i=1u;i<n_preset;++i)
        if(u32(t.preset_bag_offsets,i)<u32(t.preset_bag_offsets,i-1u) ||
           u32(t.preset_bag_offsets,i)>preset_triples)
            return NestedPathResult::Malformed;
    for(std::size_t i=1u;i<n_inst;++i)
        if(u16(t.instrument_bag_offsets,i*2u)<
           u16(t.instrument_bag_offsets,(i-1u)*2u) ||
           u16(t.instrument_bag_offsets,i*2u)>=n_ibag)
            return NestedPathResult::Malformed;
    for(std::size_t i=1u;i<n_ibag;++i)
        if(u32(t.instrument_generator_offsets,i)<
           u32(t.instrument_generator_offsets,i-1u) ||
           u32(t.instrument_generator_offsets,i)>inst_triples)
            return NestedPathResult::Malformed;
    for(std::size_t i=1u;i<t.programs.size();++i)
        if(t.programs[i].original_child_index<=
           t.programs[i-1u].original_child_index ||
           t.programs[i].original_child_index>=n_preset)
            return NestedPathResult::Malformed;
    if(t.programs.front().original_child_index!=0u ||
       t.programs.back().original_child_index!=n_preset-1u)
        return NestedPathResult::Malformed;
    for(std::size_t i=0u;i<preset_triples;++i)
        if(t.preset_generators[i*3u]>=59u)return NestedPathResult::Malformed;
    for(std::size_t i=0u;i<inst_triples;++i)
        if(t.instrument_generators[i*3u]>=59u)return NestedPathResult::Malformed;
    auto walk=[&](bool emit,NestedPathCounts& tally) noexcept ->
        NestedPathResult {
        for(std::size_t group=t.programs[row_index].original_child_index;
            group<t.programs[row_index+1u].original_child_index;++group) {
            const auto begin=u32(t.preset_bag_offsets,group);
            const auto end=u32(t.preset_bag_offsets,group+1u);
            if(begin>=end)return NestedPathResult::MissingReference;
            const std::size_t last=(end-1u)*3u;
            if(t.preset_generators[last]!=41u)
                return NestedPathResult::MissingReference;
            const auto instrument=u16(t.preset_generators,last+1u);
            if(instrument+1u>=n_inst)
                return NestedPathResult::MissingReference;
            ++tally.groups;
            const auto first_bag=u16(t.instrument_bag_offsets,instrument*2u);
            const auto last_bag=u16(t.instrument_bag_offsets,(instrument+1u)*2u);
            for(std::size_t bag=first_bag;bag<last_bag;++bag) {
                if(bag+1u>=n_ibag)return NestedPathResult::MissingReference;
                ++tally.instrument_bags;
                const auto first_gen=u32(t.instrument_generator_offsets,bag);
                const auto last_gen=u32(t.instrument_generator_offsets,bag+1u);
                for(std::size_t gen=first_gen;gen<last_gen;++gen) {
                    const auto at=gen*3u;
                    if(t.instrument_generators[at]!=53u)continue;
                    if(emit)samples[tally.sample_id_references]=static_cast<std::uint16_t>(
                        u16(t.instrument_generators,at+1u));
                    ++tally.sample_id_references;
                }
            }
        }
        return tally.sample_id_references?
            NestedPathResult::Ok:NestedPathResult::MissingReference;
    };
    NestedPathCounts measured{};
    const auto status=walk(false,measured);
    if(status!=NestedPathResult::Ok)return status;
    if(measured.sample_id_references>samples.size())
        return NestedPathResult::OutputTooSmall;
    NestedPathCounts written{};
    const auto write_status=walk(true,written);
    if(write_status==NestedPathResult::Ok)counts=written;
    return write_status;
}
} // namespace awe64::original_vxd
