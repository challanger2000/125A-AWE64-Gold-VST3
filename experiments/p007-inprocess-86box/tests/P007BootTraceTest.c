#include <stdint.h>
#include <stdio.h>
#include "P007BootTrace.h"
int main(void){
    P007BootTrace before={0}, after={0};
    if(!p007_trace_get(&before) || p007_trace_get(NULL))return 1;
    if(before.read_calls || before.write_calls)return 2;
    p007_trace_hdd_read(0u,0u,1u);
    p007_trace_hdd_read(1u,63u,2u);
    p007_trace_hdd_write(0u,7u,3u);
    if(!p007_trace_get(&after))return 3;
    if(after.read_calls!=2u||after.read_requested_sectors!=3u||
       after.write_calls!=1u||after.write_requested_sectors!=3u||
       after.last_read_lba!=63u||after.last_read_disk!=1u||
       after.last_write_lba!=7u||after.last_write_disk!=0u)return 4;
    puts("P007_ORIGINAL_DISK_REQUEST_TRACE_ATOMIC_TEST_PASS");
    return 0;
}
