#pragma once
// Host VST3 project-state codec, NOT a replacement for Creative MIDI/VXD.
// Explicit little-endian, fixed-size, allocation-free, versioned and
// checksummed. No SoundFont, ROM, device paths or proprietary data stored.
#include <algorithm>
#include <atomic>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>

namespace awe64::vst3::state {
struct BankSelection {
    std::uint16_t bank=0;
    std::uint8_t program=0;
    std::uint8_t cc7=100;
    std::uint8_t cc10=64;
    std::uint8_t cc11=127;
};
inline constexpr std::uint64_t kNoSelection=
    std::numeric_limits<std::uint64_t>::max();
inline constexpr std::array<std::uint8_t,8> kLegacyMagic{
    '1','2','5','A','P','0','0','1'};
inline constexpr std::array<std::uint8_t,8> kMagic{
    '1','2','5','A','V','S','0','2'};
inline constexpr std::size_t kStateSize=16u;

[[nodiscard]] constexpr bool valid(const BankSelection& s) noexcept {
    return s.bank<=128u && s.program<=127u && s.cc7<=127u &&
           s.cc10<=127u && s.cc11<=127u;
}
[[nodiscard]] constexpr std::uint64_t pack(const BankSelection& s) noexcept {
    return std::uint64_t(s.bank) |
           (std::uint64_t(s.program)<<16u) |
           (std::uint64_t(s.cc7)<<24u) |
           (std::uint64_t(s.cc10)<<32u) |
           (std::uint64_t(s.cc11)<<40u);
}
[[nodiscard]] constexpr std::optional<BankSelection> unpack(
    const std::uint64_t bits) noexcept {
    if(bits==kNoSelection || (bits>>48u)!=0u)return std::nullopt;
    BankSelection s{};
    s.bank=static_cast<std::uint16_t>(bits&0xffffu);
    s.program=static_cast<std::uint8_t>((bits>>16u)&0xffu);
    s.cc7=static_cast<std::uint8_t>((bits>>24u)&0xffu);
    s.cc10=static_cast<std::uint8_t>((bits>>32u)&0xffu);
    s.cc11=static_cast<std::uint8_t>((bits>>40u)&0xffu);
    if(!valid(s))return std::nullopt;
    return s;
}
[[nodiscard]] constexpr std::uint16_t crc16(
    const std::array<std::uint8_t,kStateSize>& bytes) noexcept {
    std::uint16_t crc=0xffffu;
    for(std::size_t i=0;i<kStateSize-2u;++i) {
        crc^=static_cast<std::uint16_t>(bytes[i])<<8u;
        for(unsigned bit=0;bit<8u;++bit)
            crc=static_cast<std::uint16_t>(
                (crc<<1u)^((crc&0x8000u)?0x1021u:0u));
    }
    return crc;
}
[[nodiscard]] constexpr std::array<std::uint8_t,kStateSize> encode(
    const BankSelection& selection) noexcept {
    std::array<std::uint8_t,kStateSize> bytes{};
    if(!valid(selection))return bytes;
    std::copy(kMagic.begin(),kMagic.end(),bytes.begin());
    bytes[8]=static_cast<std::uint8_t>(selection.bank);
    bytes[9]=static_cast<std::uint8_t>(selection.bank>>8u);
    bytes[10]=selection.program;
    bytes[11]=selection.cc7;
    bytes[12]=selection.cc10;
    bytes[13]=selection.cc11;
    const auto checksum=crc16(bytes);
    bytes[14]=static_cast<std::uint8_t>(checksum);
    bytes[15]=static_cast<std::uint8_t>(checksum>>8u);
    return bytes;
}
[[nodiscard]] constexpr std::optional<BankSelection> decode(
    const std::array<std::uint8_t,kStateSize>& bytes) noexcept {
    if(!std::equal(kMagic.begin(),kMagic.end(),bytes.begin()) ||
       (static_cast<std::uint16_t>(bytes[14]) |
        (static_cast<std::uint16_t>(bytes[15])<<8u)) != crc16(bytes))
        return std::nullopt;
    BankSelection s{
        static_cast<std::uint16_t>(
            bytes[8] | (static_cast<std::uint16_t>(bytes[9])<<8u)),
        bytes[10],bytes[11],bytes[12],bytes[13]};
    if(!valid(s))return std::nullopt;
    return s;
}
static_assert(unpack(pack(BankSelection{128u,127u,0u,127u,64u})).has_value());
static_assert(std::atomic<std::uint64_t>::is_always_lock_free == true,
              "Windows-x64 audio state must never use a locking atomic");
} // namespace awe64::vst3::state
