#pragma once
// P007-V: portable versioned DAW project state. Never serialize private Win95
// HDD/Creative ROMs into a DAW project. 16-byte no-heap format, with FNV1a.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace p007::state {
using Image=std::array<std::uint8_t,16>;
struct Values {std::uint8_t bank=0,program=0;std::uint16_t master=10000u;};
inline Image encode(Values v) noexcept {
 Image b{{'P','7','S','T',1u,v.bank,v.program,0u,
           static_cast<std::uint8_t>(v.master&255u),
           static_cast<std::uint8_t>(v.master>>8u),0u,0u,0u,0u,0u,0u}};
 std::uint32_t hash=2166136261u;
 for(unsigned i=0;i<12u;++i)hash=(hash^b[i])*16777619u;
 for(unsigned i=0;i<4u;++i)b[12u+i]=static_cast<std::uint8_t>(hash>>(8u*i));
 return b;
}
inline bool decode(const Image& b,Values& v) noexcept {
 if(b[0]!='P'||b[1]!='7'||b[2]!='S'||b[3]!='T'||b[4]!=1u||
    b[5]>127u||b[6]>127u||b[7]||b[10]||b[11])return false;
 const unsigned master=unsigned(b[8])+(unsigned(b[9])<<8u);
 if(master>10000u)return false;
 std::uint32_t hash=2166136261u;
 for(unsigned i=0;i<12u;++i)hash=(hash^b[i])*16777619u;
 for(unsigned i=0;i<4u;++i)
  if(b[12u+i]!=static_cast<std::uint8_t>(hash>>(8u*i)))return false;
 v={b[5],b[6],static_cast<std::uint16_t>(master)};return true;
}
inline std::uint16_t masterToBasisPoints(float v) noexcept {
 if(!std::isfinite(v))return 10000u;
 return static_cast<std::uint16_t>(std::lround(
    std::clamp(static_cast<double>(v),0.,1.)*10000.));
}
} // namespace p007::state
