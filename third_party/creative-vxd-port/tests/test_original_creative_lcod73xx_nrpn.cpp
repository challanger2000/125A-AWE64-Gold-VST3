#include "OriginalCreativeLcod73xxNrpn.hpp"
#include <array>
#include <cassert>
// CTest uses Release builds: standard assert vanishes under -DNDEBUG.
// Deliberately enforce all checks in Release and sanitizer configurations.
#undef assert
#define assert(expr) do { if (!(expr)) { std::fprintf(stderr, "LCOD73XX_FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); std::abort(); } } while(false)

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <optional>
#include <vector>
using namespace awe64::original_vxd::research;
namespace {
constexpr std::size_t row(unsigned c){return 0xf5eu+c*0x5cu;}
constexpr std::size_t chan(unsigned c){return c*0x24u;}
void wr(std::vector<std::uint8_t>& d,std::size_t off,unsigned v){
 d[off]=static_cast<std::uint8_t>(v);d[off+1]=static_cast<std::uint8_t>(v>>8u);
}
unsigned rd(const std::vector<std::uint8_t>& d,std::size_t off){
 return static_cast<unsigned>(d[off])|
        (static_cast<unsigned>(d[off+1])<<8u);
}
}
int main(){
 std::vector<std::uint8_t> d(0x151eu,0x5au);
 OriginalNrpnMemory m{d,true};
 // LSB: existing high=7F selects active parameter 14.
 constexpr unsigned c=9,p=14;
 wr(d,0x462u+chan(c),0x7f09u);
 wr(d,row(c),1u);
 d[row(c)+0x3eu+p]=1u;
 wr(d,row(c)+2u+2u*p,0x4678u);
 auto a=originalLcod7309(m,c,static_cast<std::uint8_t>(p));
 assert(a==std::optional<std::uint32_t>{0u});
 assert(rd(d,0x45eu+chan(c))==0x200u);
 assert(rd(d,0x462u+chan(c))==0x7f0eu);
 assert(rd(d,0x464u+chan(c))==0x4678u);
 // LSB 29 selects zero/disabled -> fallback 0x2000.
 assert(originalLcod7309(m,c,29u)==std::optional<std::uint32_t>{0u});
 assert(rd(d,0x464u+chan(c))==0x2000u);
 // LSB 30 is out of range and leaves output untouched; raw select retained.
 assert(originalLcod7309(m,c,30u)==std::optional<std::uint32_t>{1u});
 assert(rd(d,0x462u+chan(c))==0x7f1eu);
 assert(rd(d,0x464u+chan(c))==0x2000u);
 // High-byte 0x00 resets 30 parameter words and flags, channel parameter.
 wr(d,0x450u+chan(c),0xeeeeu);
 for(unsigned j=0;j<30;++j){d[row(c)+0x3eu+j]=1u;wr(d,row(c)+2u+j*2u,0x1100u+j);}
 assert(originalLcod740a(m,c,0u)==std::optional<std::uint32_t>{1u});
 assert(rd(d,0x462u+chan(c))==0x001eu);
 assert(rd(d,row(c))==0 && rd(d,0x450u+chan(c))==0);
 for(unsigned j=0;j<30u;++j){
  assert(d[row(c)+0x3eu+j]==0u);
  assert(rd(d,row(c)+2u+2u*j)==0x2000u);
 }
 // Set high 0x7F while low 0x1E -> rejected, then low=0 -> defaults.
 assert(originalLcod740a(m,c,0x7fu)==std::optional<std::uint32_t>{1u});
 assert(originalLcod7309(m,c,0u)==std::optional<std::uint32_t>{0u});
 assert(rd(d,0x464u+chan(c))==0x2000u);
 // Original high-byte selector 0xFF invalidates lookup, low callback masks
 // existing 0xFF high to 0x7F, unexpectedly enabling selection.
 assert(originalLcod740a(m,c,0xffu)==std::optional<std::uint32_t>{1u});
 assert(originalLcod7309(m,c,0u)==std::optional<std::uint32_t>{0u});
 assert(rd(d,0x462u+chan(c))==0x7f00u);
 // Fail-closed source and physical size validations preserve all bytes.
 auto snapshot=d;
 auto noauth=m;noauth.original_source_authenticated=false;
 assert(!originalLcod7309(noauth,c,1u).has_value());
 assert(!originalLcod740a(noauth,c,1u).has_value());
 auto shortmem=m;shortmem.device=std::span<std::uint8_t>(d).first(0x151du);
 assert(!originalLcod7309(shortmem,c,0u).has_value());
 assert(!originalLcod740a(shortmem,c,0u).has_value());
 assert(!originalLcod7309(m,16u,1u).has_value());
 assert(!originalLcod740a(m,16u,1u).has_value());
 assert(d==snapshot);
 // Original all 16 MIDI channels have separate 0x24 / 0x5C byte rows.
 for(unsigned ch=0;ch<16;++ch){
  wr(d,0x462u+chan(ch),0x7f00u);
  wr(d,row(ch),1u);
  d[row(ch)+0x3eu+29u]=1u;
  wr(d,row(ch)+2u+2u*29u,0x1000u+ch);
  assert(originalLcod7309(m,static_cast<std::uint8_t>(ch),29u)==
         std::optional<std::uint32_t>{0u});
  assert(rd(d,0x464u+chan(ch))==0x1000u+ch);
 }
 std::puts("P003_LCOD_7309_7350_740A_NRPN_STATE_PASS");
}
