#include "OriginalCreativeGuestPortCycleGroups.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace awe64::original_vxd;
#define require(c) do{if(!(c)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c);std::abort();}}while(false)

int main(){
    OriginalCreativeSourceClockProjection session{};
    session.source_session_calibrated=true;
    session.anchor_cpu_tsc=1'000u;
    session.anchor_chip_frame=200u;
    session.anchor_phase=400'000'000u-10u;
    OriginalCreativeGuestPortCycleGroups audit{session};
    // Source group: E22 index then A20 data, same captured TSC but 22
    // guest emulated cycles consumed, crossing a *hypothetical* frame.
    const auto e0=audit.append({1'000u,600});
    const auto e1=audit.append({1'000u,578});
    require(e0 && e1);
    require(e0->captured_frame==200u);
    require(e1->captured_frame==200u);
    require(e0->hypothetical_fine_frame==200u);
    require(e1->hypothetical_fine_frame==201u);
    require(e0->position_within_tsc_group==0u && e0->cycles_since_group_first_event==0u);
    require(e1->position_within_tsc_group==1u && e1->cycles_since_group_first_event==22u);
    require(e1->hypothetical_frame_differs);

    const auto e2=audit.append({1'000u,578});
    require(e2 && e2->position_within_tsc_group==2u &&
            e2->cycles_since_group_first_event==22u &&
            e2->hypothetical_fine_cpu_tsc==e1->hypothetical_fine_cpu_tsc);
    require(!audit.append({1'000u,578}));
    require(!audit.append({999u,620}));
    require(!audit.append({1'001u,622}));
    const auto next=audit.append({1'104u,600});
    require(next && next->position_within_tsc_group==0u &&
            next->captured_frame==201u);
    require(!audit.append({1'104u,601}));
    const auto next_data=audit.append({1'104u,581});
    require(next_data && next_data->cycles_since_group_first_event==19u);
    require(!audit.append({1'104u,500}));

    audit.reset();
    const auto r0=audit.append({2'000u,120});
    const auto r1=audit.append({2'000u,108});
    const auto r2=audit.append({2'000u,108});
    require(r0 && r1 && r2);
    require(r1->cycles_since_group_first_event==12u);
    require(r2->position_within_tsc_group==2u);
    require(r1->captured_frame==r0->captured_frame &&
            r2->captured_frame==r0->captured_frame);

    OriginalCreativeGuestPortCycleGroups no_clock{OriginalCreativeSourceClockProjection{}};
    require(!no_clock.append({1000u,5}));
    session.anchor_cpu_tsc=0u;
    session.anchor_chip_frame=0u;
    session.anchor_phase=0u;
    OriginalCreativeGuestPortCycleGroups overflow{session};
    require(overflow.append({std::numeric_limits<std::uint64_t>::max()-1u,200}));
    require(!overflow.append({std::numeric_limits<std::uint64_t>::max()-1u,180}));
}
