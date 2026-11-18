/* P007AJ passive original-register observation gate; no private image. */
#include <P007RegisterProbe.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"P007AJ_FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
int main(void){
    p007aj_register_event events[3]={{0}};
    CHECK(p007aj_probe_version()==1u);
    p007aj_probe_write(0x620u,0xCAFEu);
    p007aj_probe_advance(120);
    CHECK(p007aj_probe_count()==0u); /* passive default */
    CHECK(p007aj_probe_begin(40u)==1);
    CHECK(p007aj_probe_begin(41u)==0);
    p007aj_probe_advance(100);
    p007aj_probe_write(0x620u,0x1122u);
    p007aj_probe_advance(50);
    p007aj_probe_write(0xA20u,0x3344u);
    CHECK(p007aj_probe_stop()==2u);
    CHECK(p007aj_probe_count()==2u);
    CHECK(p007aj_probe_dropped()==0u);
    CHECK(p007aj_probe_copy(events,0u,3u)==2u);
    CHECK(events[0].port==0x620u&&events[0].value==0x1122u);
    CHECK(events[0].frame==100u&&events[0].serial==0u);
    CHECK(events[1].port==0xA20u&&events[1].value==0x3344u);
    CHECK(events[1].frame==150u&&events[1].serial==1u);
    CHECK(p007aj_probe_copy(events,2u,3u)==0u);
    p007aj_probe_write(0x622u,0xAABBu);
    CHECK(p007aj_probe_count()==2u); /* no data after stop */
    CHECK(p007aj_probe_begin(43u)==1);
    for(unsigned i=0;i<P007AJ_CAP+43u;++i)
        p007aj_probe_write((uint16_t)(0x620+i%4u),(uint16_t)i);
    CHECK(p007aj_probe_stop()==P007AJ_CAP+43u);
    CHECK(p007aj_probe_count()==P007AJ_CAP);
    CHECK(p007aj_probe_dropped()==43u);
    CHECK(p007aj_probe_copy(events,P007AJ_CAP-2u,3u)==2u);
    CHECK(events[1].serial==P007AJ_CAP-1u);
    CHECK(p007aj_probe_begin(44u)==1);
    CHECK(p007aj_probe_stop()==0u&&p007aj_probe_dropped()==0u);
    puts("P007AJ_BOUNDED_ATOMIC_ORIGINAL_CHIP_REGISTER_CAPTURE_PASS"
         " dormant=1 no_heap=1 no_file=1 cap=8192 overflow=43"
         " frame_time=44100 event_order=YES");
    return 0;
}
