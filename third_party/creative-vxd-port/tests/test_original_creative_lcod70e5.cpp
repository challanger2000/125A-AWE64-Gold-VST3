#include "OriginalCreativeLcod70e5.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace awe64::original_vxd::research;
namespace {
struct Op {char kind;std::uint16_t port,val;};
struct Bus {
    std::vector<Op> events{};
    std::uint16_t readValue=0xabcd;
    bool fail=false;
    static bool out8(void* p,std::uint16_t port,std::uint8_t v) noexcept{
        auto& b=*static_cast<Bus*>(p);if(b.fail)return false;
        b.events.push_back({'S',port,v});return true;
    }
    static bool in16(void* p,std::uint16_t port,std::uint16_t& v) noexcept{
        auto& b=*static_cast<Bus*>(p);if(b.fail)return false;
        b.events.push_back({'R',port,b.readValue});v=b.readValue;return true;
    }
    static bool out16(void* p,std::uint16_t port,std::uint16_t v) noexcept{
        auto& b=*static_cast<Bus*>(p);if(b.fail)return false;
        b.events.push_back({'W',port,v});return true;
    }
};
void w16(std::vector<std::uint8_t>& d,std::size_t o,unsigned v){
    d[o]=static_cast<std::uint8_t>(v);d[o+1]=static_cast<std::uint8_t>(v>>8u);
}
void w32(std::vector<std::uint8_t>& d,std::size_t o,unsigned v){
    w16(d,o,v);w16(d,o+2u,v>>16u);
}
std::uint16_t r16(std::vector<std::uint8_t> const& d,std::size_t o){
    return static_cast<std::uint16_t>(d[o]|
           (static_cast<unsigned>(d[o+1u])<<8u));
}
void check(bool p,const char* s){
    if(!p){std::fprintf(stderr,"LCOD_70E5_FAIL %s\n",s);std::abort();}
}
}
int main(){
    std::vector<std::uint8_t> device(8192u,0u);
    std::array<std::uint8_t,180> entries{};
    std::array<std::uint16_t,128> lookups{};
    std::array<std::uint16_t,64> output{};
    // ALL table, chip, and device values below are SYNTHETIC.
    entries[0]=4;entries[1]=1;entries[2]=0;entries[3]=4;
    entries[6]=5;entries[7]=1;entries[8]=0x60;entries[9]=4;
    entries[10]=1;
    entries[12]=8;entries[13]=1;entries[14]=0x80;entries[15]=4;
    entries[16]=0;
    for(std::size_t k=0;k<lookups.size();++k)
        lookups[k]=static_cast<std::uint16_t>(65535u-k*500u);
    w16(device,4,0xe20);w16(device,6,4);
    w16(device,0x42,0x0201);
    w16(device,0x42+0x20,0xff00);
    w16(device,0x42+2*0x20,0x0201);
    w16(device,0x42+3*0x20,0x0300);
    const std::size_t ch=0x442+2*0x24;
    w16(device,ch+0x20,0x7f01);
    w16(device,ch+0x22,0x2004);
    device[ch+0xa]=2;device[ch+0xb]=3;
    Bus bus;
    awe64::original_vxd::PortIo io{};
    io.opaque=&bus;io.out8=Bus::out8;
    io.in16=Bus::in16;io.out16=Bus::out16;
    Lcod70e5Inputs data{device,entries,lookups,true,io};
    check(originalLcod70e5(data,2u)==Lcod70e5Status::Applied,
          "parameter 5 result");
    check(bus.events.size()==8,"two voices read and write");
    for(std::size_t n=0;n<8;n+=4){
        check(bus.events[n].kind=='S'&&bus.events[n].port==0xe22&&
              bus.events[n].val==static_cast<unsigned>(0x60+(n/4)*2),
              "selector read");
        check(bus.events[n+1].kind=='R'&&bus.events[n+1].port==0xe20,
              "hardware read16");
        check(bus.events[n+2].kind=='S',"selector before write");
        check(bus.events[n+3].kind=='W'&&bus.events[n+3].port==0xe20&&
              bus.events[n+3].val==0x09cdu,"preserve hardware low byte");
    }
    const std::size_t row=0xf5e + 2*0x5c;
    check(r16(device,row)==1 && device[row+0x3e + 1]==1 &&
          r16(device,row+4)==0x2004,"source record state");
    // Original LCOD+0x71E4 parameter 4 branch: 16-bit OUT, no IN.
    w16(device,ch+0x20,0x7f00);
    w16(device,0x42+0xe,300);
    w16(device,0x42+2*0x20+0xe,301);
    w32(device,ch+0x14,5000);
    bus.events.clear();
    check(originalLcod70e5(data,2u)==Lcod70e5Status::Applied,
          "parameter 4 result");
    check(r16(device,ch+0xe)==13,"signed shift/division");
    check(bus.events.size()==4,"two OUT16 transactions");
    check(bus.events[1].kind=='W'&&bus.events[1].val==5313,
          "first direct hardware value");
    check(bus.events[3].kind=='W'&&bus.events[3].val==5314,
          "second direct hardware value");
    // Preserve hardware high byte where record flag is 0.
    w16(device,ch+0x20,0x7f02);
    bus.events.clear();
    check(originalLcod70e5(data,2u)==Lcod70e5Status::Applied,
          "parameter 8 result");
    check(bus.events.size()==8 && bus.events[3].val==0xab08u,
          "preserve hardware high byte");
    // Original high-byte selector branch: parameter export, no chip I/O.
    const std::size_t row2=0xf5e + 2*0x5c;
    device[row2+0x3e]=1;device[row2+0x3e + 1]=1;
    w16(device,row2+2,0x2005);w16(device,row2+4,0x2006);
    bus.events.clear();
    check(originalLcod70e5(data,0x8002u,output)==Lcod70e5Status::Applied,
          "parameter export");
    check(output[4]==5 && output[5]==6 && bus.events.empty(),
          "export values no port writes");
    // Policy checks are not source authentication; fail closed.
    auto bad=data;bad.owner_source_authenticated=false;
    check(originalLcod70e5(bad,2u)==Lcod70e5Status::RejectedSource,
          "missing source attestation");
    bad=data;
    bad.original_parameter_entries=
        std::span<const std::uint8_t>(entries).first(5);
    check(originalLcod70e5(bad,2u)==Lcod70e5Status::RejectedSource,
          "truncated parameter table");
    check(originalLcod70e5(data,100u)==Lcod70e5Status::Invalid,
          "invalid channel selector");
    w16(device,ch+0x20,0x7f1f);
    check(originalLcod70e5(data,2u)==Lcod70e5Status::Invalid,
          "invalid original parameter slot");
    std::puts("LCOD_70E5_REGISTER_READ_WRITE_AND_SOURCE_STATE_PASS");
}
