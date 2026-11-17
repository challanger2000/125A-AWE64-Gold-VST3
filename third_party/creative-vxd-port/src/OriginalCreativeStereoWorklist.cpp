#include "OriginalCreativeStereoWorklist.hpp"
#include <array>
#include <cstdint>

namespace awe64::original_vxd {
namespace {
[[nodiscard]] constexpr std::uint16_t word(Descriptor const& d,
                                            std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<unsigned>(d[offset]) |
        (static_cast<unsigned>(d[offset+1]) << 8u));
}
} // namespace

Worklist original_stereo_worklist(std::span<const Descriptor> d) noexcept {
    Worklist result;
    if(d.empty() || d.size()>max_note_descriptors) {
        result.status = d.size()>max_note_descriptors?
            WorkStatus::TooManyDescriptors:WorkStatus::BadInput;
        return result;
    }
    // The original marks the two matched descriptor sampleID fields with
    // 0xFFFF in its *temporary* list. Here a separate bitmask preserves
    // caller-owned SF2/descriptor bytes without changing behavior.
    std::array<bool,max_note_descriptors> consumed{};
    for(std::size_t i=0;i<d.size();++i) {
        if(consumed[i])continue;
        if((word(d[i],0x8c)&0x0002u)==0u)continue; // rightSample bit
        const std::uint16_t target=word(d[i],0x8e);
        std::size_t partner=d.size();
        for(std::size_t j=0;j<d.size();++j) {
            if(j==i || consumed[j])continue;
            if(word(d[j],0x6a)==target) {partner=j;break;}
        }
        if(partner==d.size())continue; // source second pass as type 1
        consumed[i]=consumed[partner]=true;
        result.items[result.count++]={static_cast<std::uint8_t>(partner),4u,
                                        static_cast<std::uint8_t>(i)};
        result.items[result.count++]={static_cast<std::uint8_t>(i),2u,
                                        static_cast<std::uint8_t>(partner)};
        ++result.stereo_pairs;
    }
    for(std::size_t i=0;i<d.size();++i) {
        if(!consumed[i])result.items[result.count++]={
            static_cast<std::uint8_t>(i),1u,no_voice};
    }
    result.status=WorkStatus::Ok;
    return result;
}

bool original_assign_stereo_pair(
    WorkItem const partner, WorkItem const right,
    Allocation const& selected,
    PhysicalWorkItem& first, PhysicalWorkItem& second) noexcept {
    if(partner.process_type!=4u || right.process_type!=2u ||
       partner.source_index==right.source_index ||
       partner.paired_source!=right.source_index ||
       right.paired_source!=partner.source_index ||
       selected.status!=Status::Found ||
       selected.first>=physical_voices-1u ||
       selected.second!=static_cast<unsigned>(selected.first)+1u)
        return false;
    first={partner,selected.second};
    second={right,selected.first};
    return true;
}
} // namespace awe64::original_vxd
