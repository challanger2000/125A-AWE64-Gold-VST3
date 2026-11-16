#include "OriginalCreativeProgramSelection.hpp"
#include "OriginalCreativeVerifiedSf2Bank.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"PROGRAM_SELECT_FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
struct Directory {
    struct Item {std::uint8_t bank,program;};
    std::array<Item,8> entries{};
    std::size_t count=0;
    void put(std::uint8_t b,std::uint8_t p){entries[count++]={b,p};}
    static bool contains(const void* ctx,std::uint8_t bank,std::uint8_t program) noexcept {
        auto& d=*static_cast<Directory const*>(ctx);
        for(std::size_t i=0;i<d.count;++i)if(d.entries[i].bank==bank &&
            d.entries[i].program==program)return true;
        return false;
    }
};
int main(){
    Directory d{};
    d.put(128,7); d.put(0,7);d.put(12,7);d.put(0,9);
    auto select=[&](std::uint8_t b,std::uint8_t p){
        return original_melodic_program_bank(b,p,&d,Directory::contains);
    };
    // Original +0x6E71: requested bank 0 actually searches 128 first.
    auto a=select(0,7);CHECK(a.found && a.selected_bank==128 && a.fallback_pass==0);
    a=select(12,7);CHECK(a.found && a.selected_bank==12 && a.fallback_pass==0);
    a=select(64,7);CHECK(a.found && a.selected_bank==128 && a.fallback_pass==1);
    a=select(128,7);CHECK(a.found && a.selected_bank==128 && a.fallback_pass==0);
    a=select(64,9);CHECK(a.found && a.selected_bank==0 && a.fallback_pass==2);
    a=select(0,9);CHECK(a.found && a.selected_bank==0 && a.fallback_pass==1);
    a=select(64,24);CHECK(!a.found);
    CHECK(!original_melodic_program_bank(0,7,&d,nullptr).found);
    // The resolved bank applies to all zones within a patch, not to each
    // individual key; this test validates priority, not zone conversion.
    std::puts("P003_ORIGINAL_PROGRAM_BANK_FALLBACK_CONTRACT_PASS");
}
