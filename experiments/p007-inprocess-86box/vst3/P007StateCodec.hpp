#pragma once
// P007AA: integrity-checked VST3 project recall.
// v1 (16 bytes) is still accepted, v2 (44 bytes) stores 12 original
// Creative NRPN values. Never serialize ROM, guest disk or private drivers.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
namespace p007::state {
using LegacyImage=std::array<std::uint8_t,16>;
using Image=std::array<std::uint8_t,44>;
struct Values {
 std::uint8_t bank=0,program=0;
 std::uint16_t master=10000u;
 std::array<std::uint16_t,12> modulation{};
};
inline std::uint32_t checksum(const std::uint8_t* data,unsigned length) noexcept {
 std::uint32_t hash=2166136261u;
 for(unsigned i=0;i<length;++i)hash=(hash^data[i])*16777619u;
 return hash;
}
template<std::size_t N>
inline void writeHash(std::array<std::uint8_t,N>& bytes) noexcept {
 const auto hash=checksum(bytes.data(),static_cast<unsigned>(N-4u));
 for(unsigned i=0;i<4u;++i)bytes[N-4u+i]=static_cast<std::uint8_t>(hash>>(8u*i));
}
template<std::size_t N>
inline bool validHash(const std::array<std::uint8_t,N>& bytes) noexcept {
 const auto hash=checksum(bytes.data(),static_cast<unsigned>(N-4u));
 for(unsigned i=0;i<4u;++i)
  if(bytes[N-4u+i]!=static_cast<std::uint8_t>(hash>>(8u*i)))return false;
 return true;
}
inline LegacyImage encodeLegacy(const Values& v) noexcept {
 LegacyImage bytes{{'P','7','S','T',1u,v.bank,v.program,0u,
     static_cast<std::uint8_t>(v.master&255u),
     static_cast<std::uint8_t>(v.master>>8u),0u,0u,0u,0u,0u,0u}};
 writeHash(bytes);return bytes;
}
inline Image encode(const Values& v) noexcept {
 Image bytes{};
 bytes[0]='P';bytes[1]='7';bytes[2]='S';bytes[3]='T';bytes[4]=2u;
 bytes[5]=v.bank;bytes[6]=v.program;
 bytes[8]=static_cast<std::uint8_t>(v.master&255u);
 bytes[9]=static_cast<std::uint8_t>(v.master>>8u);
 for(unsigned i=0;i<12u;++i) {
  bytes[12u+2u*i]=static_cast<std::uint8_t>(v.modulation[i]&255u);
  bytes[13u+2u*i]=static_cast<std::uint8_t>(v.modulation[i]>>8u);
 }
 writeHash(bytes);return bytes;
}
inline bool decode(const LegacyImage& bytes,Values& out) noexcept {
 if(bytes[0]!='P'||bytes[1]!='7'||bytes[2]!='S'||bytes[3]!='T'||
    bytes[4]!=1u||bytes[5]>127u||bytes[6]>127u||
    bytes[7]||bytes[10]||bytes[11]||!validHash(bytes))return false;
 const unsigned master=unsigned(bytes[8])+(unsigned(bytes[9])<<8u);
 if(master>10000u)return false;
 Values v{};
 v.bank=bytes[5];v.program=bytes[6];v.master=static_cast<std::uint16_t>(master);
 out=v;return true;
}
inline bool decode(const Image& bytes,Values& out) noexcept {
 if(bytes[0]!='P'||bytes[1]!='7'||bytes[2]!='S'||bytes[3]!='T'||
    bytes[4]!=2u||bytes[5]>127u||bytes[6]>127u||
    bytes[7]||bytes[10]||bytes[11]||
    bytes[36]||bytes[37]||bytes[38]||bytes[39]||
    !validHash(bytes))return false;
 Values v{};
 v.bank=bytes[5];v.program=bytes[6];
 v.master=static_cast<std::uint16_t>(unsigned(bytes[8])+(unsigned(bytes[9])<<8u));
 if(v.master>10000u)return false;
 for(unsigned i=0;i<12u;++i) {
  const unsigned amount=unsigned(bytes[12u+2u*i])+
                        (unsigned(bytes[13u+2u*i])<<8u);
  if(amount>10000u)return false;
  v.modulation[i]=static_cast<std::uint16_t>(amount);
 }
 out=v;return true;
}
inline std::uint16_t masterToBasisPoints(float value) noexcept {
 if(!std::isfinite(value))return 10000u;
 return static_cast<std::uint16_t>(std::lround(
   std::clamp(static_cast<double>(value),0.,1.)*10000.));
}
inline std::uint16_t normalizedToBasisPoints(double value) noexcept {
 if(!std::isfinite(value))return 0u;
 return static_cast<std::uint16_t>(std::lround(
   std::clamp(value,0.,1.)*10000.));
}
} // namespace p007::state
