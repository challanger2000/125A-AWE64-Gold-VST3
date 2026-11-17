#pragma once
// Research-only C++23 translation: original SBAWE.VXD 4.38.0.2
// LCOD+0x70E5..+0x7308. Its LE fixups target PCOD+0x37BA and
// the original LCOD+0x3CB1/0x3CE6 16-bit OUT/IN helpers.
// NO original Creative table, binary, ROM, or SoundFont bytes embedded.
// Not authorized for productive VST3 activation.
#include "OriginalCreativePcodParameterTransform.hpp"
#include "OriginalCreativeVxdLive.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace awe64::original_vxd::research {
enum class Lcod70e5Status : std::uint8_t {
    Applied, RejectedSource, Invalid, PortError
};
struct Lcod70e5Inputs {
    std::span<std::uint8_t> device{};
    std::span<const std::uint8_t> original_parameter_entries{}; // 30 x 6
    PcodTable original_lookup{}; // 128 original 16-bit words
    bool owner_source_authenticated=false; // assertion, NOT verification
    PortIo io{};
};
namespace detail70e5 {
[[nodiscard]] inline bool within(std::span<const std::uint8_t> data,
                                 std::size_t start,std::size_t length) noexcept {
    return start<=data.size() && length<=data.size()-start;
}
[[nodiscard]] inline std::uint16_t read16(std::span<const std::uint8_t> data,
                                           std::size_t off) noexcept {
    return static_cast<std::uint16_t>(static_cast<unsigned>(data[off])|
           (static_cast<unsigned>(data[off+1u])<<8u));
}
[[nodiscard]] inline std::uint32_t read32(std::span<const std::uint8_t> data,
                                           std::size_t off) noexcept {
    return static_cast<std::uint32_t>(read16(data,off))|
           (static_cast<std::uint32_t>(read16(data,off+2u))<<16u);
}
inline void write16(std::span<std::uint8_t> data,std::size_t off,
                    std::uint16_t value) noexcept {
    data[off]=static_cast<std::uint8_t>(value);
    data[off+1u]=static_cast<std::uint8_t>(value>>8u);
}
[[nodiscard]] constexpr std::int16_t signed16(std::uint16_t x) noexcept {
    return std::bit_cast<std::int16_t>(x);
}
// Exact selector/data port routing of original LCOD+0x3CB1/+0x3CE6.
// Each transaction emits an OUT8 selector before IN16/OUT16 data.
[[nodiscard]] inline bool port16(Lcod70e5Inputs const& in,
                                 std::uint16_t selector,bool read,
                                 std::uint16_t& value) noexcept {
    auto const device=std::span<const std::uint8_t>(in.device);
    const auto portOffset=static_cast<std::size_t>((selector>>8u)&~1u);
    if(!within(device,4u,2u) || !within(device,portOffset,2u) ||
       !in.io.out8 || (read?!in.io.in16:!in.io.out16))return false;
    const auto indexPort=static_cast<std::uint16_t>(read16(device,4u)+2u);
    const auto dataPort=static_cast<std::uint16_t>(
        read16(device,portOffset)|((selector&0x0100u)>>7u));
    if(!in.io.out8(in.io.opaque,indexPort,static_cast<std::uint8_t>(selector)))
        return false;
    if(read)return in.io.in16(in.io.opaque,dataPort,value);
    return in.io.out16(in.io.opaque,dataPort,value);
}
}

// originalSelector is the ORIGINAL +0x0C stack argument, not a MIDI byte.
// High-byte bit7: export parameters; otherwise channel/voice register path.
[[nodiscard]] inline Lcod70e5Status originalLcod70e5(
    Lcod70e5Inputs const& in,std::uint32_t originalSelector,
    std::span<std::uint16_t> output={}) noexcept {
    using namespace detail70e5;
    auto d=std::span<const std::uint8_t>(in.device);
    auto const t=in.original_parameter_entries;
    if(!in.owner_source_authenticated || t.size()!=30u*6u ||
       !within(d,6u,2u))return Lcod70e5Status::RejectedSource;

    // Original LCOD+0x70EB: test BYTE [ebp+0x0D],0x80.
    if((originalSelector&0x8000u)!=0u) {
        const auto channel=static_cast<std::size_t>(originalSelector&0x1fu);
        const auto row=0x0F5Eu+channel*0x5cu;
        if(!within(d,row+0x3eu,30u) ||
           !within(d,row+2u,60u))return Lcod70e5Status::RejectedSource;
        std::array<std::uint16_t,30> next{};
        std::array<std::uint8_t,30> keys{};
        std::size_t n=0u;
        for(std::size_t j=0;j<30u;++j) {
            if(d[row+0x3eu+j]!=1u)continue;
            const auto code=t[6u*j];
            if(code>=output.size())return Lcod70e5Status::RejectedSource;
            const auto old=static_cast<std::int32_t>(
                signed16(read16(d,row+2u+2u*j)))-0x2000;
            const auto result=originalPcod37ba(code,old,4,in.original_lookup);
            if(!result)return Lcod70e5Status::RejectedSource;
            keys[n]=code;
            next[n]=static_cast<std::uint16_t>(*result);
            ++n;
        }
        // Stage before commit so a missing authentic source never partially
        // overwrites the owner's export buffer.
        for(std::size_t j=0;j<n;++j)output[keys[j]]=next[j];
        return Lcod70e5Status::Applied;
    }

    const auto index=static_cast<std::size_t>(originalSelector);
    if(index>31u)return Lcod70e5Status::Invalid;
    const auto channelRow=0x442u+index*0x24u;
    const auto controlRow=0x0F5Eu+index*0x5cu;
    if(!within(d,channelRow,0x24u) ||
       !within(d,controlRow,0x5cu))return Lcod70e5Status::RejectedSource;
    const auto selected=read16(d,channelRow+0x20u);
    const auto slot=static_cast<unsigned>(selected&0x7fu);
    if(slot>=30u || (selected>>8u)!=127u)
        return Lcod70e5Status::Invalid; // original EAX=1
    const auto sourceValue=read16(d,channelRow+0x22u);
    const auto entry=static_cast<std::size_t>(slot)*6u;
    const auto capacity=static_cast<unsigned>(read16(d,6u));
    if(capacity>32u || (capacity!=0u &&
       !within(d,0x42u,capacity*0x20u)))
        return Lcod70e5Status::RejectedSource;

    // Original LCOD+0x71B1..+0x71C0 commits all three source fields
    // before testing the table entry's enabled flag.
    if(t[entry+1u]==0u) {
        write16(in.device,controlRow,1u);
        in.device[controlRow+0x3eu+slot]=1u;
        write16(in.device,controlRow+2u+2u*slot,sourceValue);
        return Lcod70e5Status::Applied;
    }
    const auto code=static_cast<unsigned>(t[entry]);
    const auto reg=read16(t,entry+2u);
    const auto byteMergeFlag=t[entry+4u]!=0u;
    const auto shifted=signed16(static_cast<std::uint16_t>(sourceValue-0x2000u));
    std::uint16_t parameter=0u;
    if(code==4u) {
        const auto product=static_cast<std::int32_t>(shifted)*4096;
        parameter=static_cast<std::uint16_t>(product/1200);
    } else {
        const auto transformed=originalPcod37ba(
            code,shifted,4,in.original_lookup);
        if(!transformed)return Lcod70e5Status::RejectedSource;
        parameter=static_cast<std::uint16_t>(*transformed);
        if(code==5u) {
            const auto sum=static_cast<std::uint16_t>(
                static_cast<unsigned>(parameter)+
                static_cast<unsigned>(d[channelRow+0x0au])+
                static_cast<unsigned>(d[channelRow+0x0bu]));
            parameter=static_cast<std::uint16_t>(
                signed16(sum)>127?127u:sum);
            parameter=static_cast<std::uint16_t>(parameter<<8u);
        } else if(byteMergeFlag) {
            parameter=static_cast<std::uint16_t>(parameter<<8u);
        }
    }

    write16(in.device,controlRow,1u);
    in.device[controlRow+0x3eu+slot]=1u;
    write16(in.device,controlRow+2u+2u*slot,sourceValue);
    if(code==4u)write16(in.device,channelRow+0x0eu,parameter);

    for(unsigned voice=0u;voice<capacity;++voice) {
        const auto row=0x42u+voice*0x20u;
        const auto associated=read16(d,row);
        if((associated>>8u)==0xffu || (associated>>8u)!=index)
            continue;
        if(code==4u) {
            const std::uint32_t valueBits=
                static_cast<std::uint32_t>(read16(d,row+0x0eu))+
                read32(d,channelRow+0x14u)+
                static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(signed16(parameter)));
            const auto v=x86s32(valueBits);
            std::uint16_t outputValue=static_cast<std::uint16_t>(
                v>65535?65535u:valueBits);
            const auto selector=static_cast<std::uint16_t>(0x0400u|voice);
            if(!port16(in,selector,false,outputValue))
                return Lcod70e5Status::PortError;
        } else {
            const auto selector=static_cast<std::uint16_t>(reg|voice);
            std::uint16_t current=0u;
            if(!port16(in,selector,true,current))
                return Lcod70e5Status::PortError;
            std::uint16_t updated=byteMergeFlag
                ?static_cast<std::uint16_t>((parameter&0xff00u)|
                                             (current&0x00ffu))
                :static_cast<std::uint16_t>((current&0xff00u)|
                                             (parameter&0x00ffu));
            if(!port16(in,selector,false,updated))
                return Lcod70e5Status::PortError;
        }
    }
    return Lcod70e5Status::Applied;
}
} // namespace awe64::original_vxd::research
