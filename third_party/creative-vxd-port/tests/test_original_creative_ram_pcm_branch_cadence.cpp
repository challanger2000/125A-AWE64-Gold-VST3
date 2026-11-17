#include "OriginalCreativeSf2RamUpload.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <array>
using awe64::original_vxd::OriginalCreativePcmFilter;
using awe64::original_vxd::OriginalCreativePcmX86Path;
#define require(x) do{if(!(x)){std::fprintf(stderr,"failed: %d %s\n",__LINE__,#x);std::abort();}}while(false)

int main() {
    OriginalCreativePcmFilter filter{};
    require(filter.will_take_early_zero_branch(123));
    require(filter.original_x86_path(123)==OriginalCreativePcmX86Path::GuardedZero);
    require(filter.observed_control_flow_pcm_loop_delta_tsc(123)==104u);
    require(filter.observed_typical_pcm_loop_delta_tsc(123)==100u);
    require(filter.next(123)==0);
    require(filter.will_take_early_zero_branch(456));
    require(filter.original_x86_path(456)==OriginalCreativePcmX86Path::GuardedZero);
    require(filter.observed_control_flow_pcm_loop_delta_tsc(456)==104u);
    require(filter.next(456)==0);
    require(!filter.will_take_early_zero_branch(0));
    require(filter.observed_typical_pcm_loop_delta_tsc(0)==104u);
    require(filter.original_x86_path(0)==OriginalCreativePcmX86Path::OrdinaryNonzeroOlder);
    require(filter.observed_control_flow_pcm_loop_delta_tsc(0)==104u);
    const auto before=filter;
    const auto out=filter.next(0);
    auto restored=before;
    require(out==restored.next(0));
    filter={};
    for(unsigned i=0;i<1000;++i) {
        require(filter.will_take_early_zero_branch(0));
        require(filter.observed_typical_pcm_loop_delta_tsc(0)==100u);
        require(filter.original_x86_path(0)==OriginalCreativePcmX86Path::DirectZero);
        require(filter.observed_control_flow_pcm_loop_delta_tsc(0)==100u);
        require(filter.next(0)==0);
    }
    filter={};
    filter.older=1;filter.older2=6;filter.older3=1;filter.recent=0;
    require(!filter.will_take_early_zero_branch(0));
    require(filter.observed_typical_pcm_loop_delta_tsc(0)==104u);
    require(filter.next(0)==0);

    filter={};filter.older=0;filter.recent=1;
    filter.older2=2;filter.older3=3;
    require(!filter.will_take_early_zero_branch(0));
    require(filter.original_x86_path(0)==OriginalCreativePcmX86Path::OrdinaryNonzeroOlder2);
    require(filter.observed_control_flow_pcm_loop_delta_tsc(0)==110u);
    filter.older2=0;filter.older3=3;
    require(!filter.will_take_early_zero_branch(0));
    require(filter.original_x86_path(0)==OriginalCreativePcmX86Path::OrdinaryNonzeroOlder3);
    require(filter.observed_control_flow_pcm_loop_delta_tsc(0)==114u);
    filter.older3=0;
    require(filter.will_take_early_zero_branch(0));
    require(filter.original_x86_path(0)==OriginalCreativePcmX86Path::GuardedZero);
    require(filter.observed_control_flow_pcm_loop_delta_tsc(0)==104u);
    const auto saved=filter;
    const auto diagnostic=filter.observed_control_flow_pcm_loop_delta_tsc(1234);
    require(diagnostic==104u);
    require(filter.recent==saved.recent && filter.older==saved.older &&
            filter.older2==saved.older2 && filter.older3==saved.older3);
}
