#include "OriginalCreativePcodParameterTransform.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace awe64::original_vxd::research;
// Independent dispatch model from the actual original LE byte-table and
// 23 fixup targets; original Creative lookup-table bytes are NOT included.
static constexpr std::array<unsigned,44> route{
  0,0,0,1,2,3,3,22,4,5,22,22,22,22,22,22,
  6,7,8,9,10,11,12,13,14,15,22,22,16,17,22,18,
  19,20,22,22,22,22,22,22,22,22,22,21
};
static constexpr std::array<unsigned,23> handler{
  0,1,2,1,1,2,3,1,3,1,3,4,5,6,7,6,3,4,8,7,6,9,10
};
static std::int32_t wrap(std::int64_t x){
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(x));
}
static std::int16_t word(std::int64_t x){
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(x));
}
static std::int32_t lookup(std::uint16_t x,
    std::array<std::uint16_t,128> const& t){
    if(x==0u)return 127;
    for(std::size_t k=0u;k<128u;++k)
        if(t[k]<x)return static_cast<std::int32_t>(k);
    return 127;
}
static std::int32_t model(unsigned op,std::int32_t x,std::int32_t a,
    std::array<std::uint16_t,128> const& t) {
    const auto index=op>=5u&&op<=48u?route[op-5u]:22u;
    switch(handler[index]){
      case 0:return x>127?127:x<-127?-127:x;
      case 1:return wrap(static_cast<std::int64_t>(x)*2);
      case 2:return x/8;
      case 3:{
        auto p=wrap(static_cast<std::int64_t>(word(a))*x);
        p=wrap(static_cast<std::int64_t>(p)*1000);
        return 32768-word(p/725);
      }
      case 4:return lookup(static_cast<std::uint16_t>(x),t);
      case 5:{
        auto y=x/-92+127;
        return y>127?127:y<0?0:y;
      }
      case 6:return lookup(static_cast<std::uint16_t>(
                   wrap(static_cast<std::int64_t>(word(a))*x)),t);
      case 7:{
        auto y=wrap(static_cast<std::int64_t>(x)*100)/75;
        return y>127?127:y<0?0:y;
      }
      case 8:return wrap(static_cast<std::int64_t>(word(a))*x);
      case 9:return wrap(static_cast<std::int64_t>(
                   wrap(static_cast<std::uint32_t>(127)-
                        static_cast<std::uint32_t>(x)))*75)/10;
      case 10:return x;
    }
    std::abort();
}
int main(){
    std::array<std::uint16_t,128> fake{};
    // Deliberately synthetic: not the 128 original Creative words.
    for(std::size_t k=0u;k<128u;++k)
        fake[k]=static_cast<std::uint16_t>(65535u-k*503u);
    unsigned long long count=0u;
    for(unsigned op=0u;op<=58u;++op) {
        for(int x=-32768;x<=32767;++x) {
            for(int a:{-128,-1,0,1,4,127}) {
                auto got=originalPcod37ba(op,x,a,fake);
                if(!got || *got!=model(op,x,a,fake)){
                    std::fprintf(stderr,"PCOD_37BA_FAIL op=%u x=%d a=%d\n",
                        op,x,a);
                    return 1;
                }
                ++count;
            }
        }
    }
    for(int x:{std::numeric_limits<std::int32_t>::min(),-2147483647,
               -65536,-32769,32768,65536,2147483646,
               std::numeric_limits<std::int32_t>::max()}) {
        for(unsigned op=0u;op<=58u;++op) {
            for(int a:{-32768,-128,-1,0,1,127,32767}) {
                const auto got=originalPcod37ba(op,x,a,fake);
                if(!got || *got!=model(op,x,a,fake))return 2;
                ++count;
            }
        }
    }
    for(unsigned op:{26u,28u,30u,34u,38u})
        if(originalPcod37ba(op,100,4,{}))return 3;
    if(!originalPcod37ba(8,100,4,{}))return 4;
    std::printf("PCOD_37BA_BRANCH_AND_INTEGER_TEST_PASS cases=%llu\n",count);
}
