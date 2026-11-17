#include "OriginalCreativeRamScratchVoices.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace awe64::original_vxd {
namespace {
[[nodiscard]] bool io_valid(PortIo const io) noexcept {
    return io.out8 && io.out16 && io.out32 && io.in16 && io.in32;
}
[[nodiscard]] bool word(PortIo io,std::uint8_t selector,
                        std::uint16_t value) noexcept {
    return io.out8(io.opaque,0xe22u,selector) &&
           io.out16(io.opaque,0xa20u,value);
}
[[nodiscard]] bool wide(PortIo io,std::uint8_t selector,
                        std::uint16_t data_port,
                        std::uint32_t value) noexcept {
    return io.out8(io.opaque,0xe22u,selector) &&
           io.out32(io.opaque,data_port,value);
}
}

bool OriginalCreativeRamScratchCycle::before_block() noexcept {
    if(active || faulted || !device ||
       device->physical_capacity==0u ||
       device->physical_capacity>physical_voices || !io_valid(io))return false;
    selected.fill(no_voice);
    active=true;
    for(std::size_t n=0;n<selected.size();++n) {
        // x86 PCOD+0x26C7 calls the source voice allocator with an FF
        // result sentinel, then visits up to ten actual selected voices.
        const auto found=allocate_mono_mode0(*device,io);
        if(found.status==Status::NoCandidate)continue;
        if(found.status!=Status::Found ||
           found.first>=device->physical_capacity){faulted=true;return false;}
        const auto id=found.first;
        selected[n]=id;
        auto& v=device->voices[id];
        // Original allocator dispatcher LCOD+0x3D6F writes a temporary
        // channel/key marker (FF:00), clears cached sample-end (+0x4C)
        // and candidate penalty (+0x5C) BEFORE the NEXT allocator call.
        // Without these writes, the first idle voice is wrongly chosen
        // ten times; the original instead takes physical 0,3,..,27 in
        // the unoccupied 32-voice original-bank test.
        v.key_marker=0xff00u;
        v.sample_end=0u;
        v.pair_penalty=false;
        // Original voice+0x59 (device's 0x20-byte voice record +0x17)
        // decrements its mutable active-voice count when already set.
        if(v.blocked)--device->mode_count;
        // PCOD+0x26AF also needs the already-selected voice's VTFT reset,
        // between allocator DCYSUSV=FF and explicit scratch setup.
        if(!wide(io,static_cast<std::uint8_t>(0x60u|id),0x620u,0x0000ffffu) ||
           !word(io,static_cast<std::uint8_t>(0xa0u|id),0x0080u) ||
           !wide(io,static_cast<std::uint8_t>(0x60u|id),0x620u,0u) ||
           !wide(io,static_cast<std::uint8_t>(0x40u|id),0x620u,0u) ||
           !wide(io,static_cast<std::uint8_t>(0xc0u|id),0x620u,0u) ||
           !wide(io,static_cast<std::uint8_t>(0xe0u|id),0x620u,0u) ||
           !wide(io,static_cast<std::uint8_t>(0x20u|id),0x620u,0x40000000u) ||
           !wide(io,id,0x620u,0x40000000u) ||
           !wide(io,id,0xa20u,0x06000000u)) {
            faulted=true;return false;
        }
        ++selected_voices;
    }
    return true;
}

bool OriginalCreativeRamScratchCycle::after_block() noexcept {
    if(!active || faulted || !device || !io_valid(io))return false;
    // PCOD+0x283D restores one voice at a time in ORIGINAL slot order.
    // A 0xFF (no candidate) is skipped, not converted into voice31.
    for(auto const id:selected) {
        if(id==no_voice)continue;
        if(id>=device->physical_capacity ||
           !wide(io,id,0xa20u,0u) ||
           !wide(io,static_cast<std::uint8_t>(0xc0u|id),0x620u,0x1d8u) ||
           !wide(io,static_cast<std::uint8_t>(0xe0u|id),0x620u,0x1e0u) ||
           !wide(io,static_cast<std::uint8_t>(0x20u|id),0x620u,0x48000000u)) {
            faulted=true;return false;
        }
        auto& v=device->voices[id];
        v.key_marker=0x0ffeu;  // original source [+voice*0x20+0x42]
        v.blocked=true;       // original source [+voice*0x20+0x59]=1
        ++device->mode_count;
    }
    active=false;
    ++cycles_completed;
    return true;
}

OriginalCreativeRamBlockHooks OriginalCreativeRamScratchCycle::hooks() noexcept {
    return OriginalCreativeRamBlockHooks{
        this,
        [](void* p) noexcept -> bool {
            return static_cast<OriginalCreativeRamScratchCycle*>(p)->before_block();
        },
        [](void* p) noexcept -> bool {
            return static_cast<OriginalCreativeRamScratchCycle*>(p)->after_block();
        }
    };
}
} // namespace awe64::original_vxd
