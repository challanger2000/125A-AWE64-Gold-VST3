#include "CreativeChipPortBridge.h"
#include <cstdint>
namespace awe64::creative {
namespace {
bool validPair(const OriginalWin95VxdPortWrite& ptr,
               const OriginalWin95VxdPortWrite& data) noexcept {
    if(ptr.port!=0xE22u || ptr.access_width_bits!=8u ||
       ptr.value>0xffu || (ptr.value&0x1fu)>=30u)
       return false;
    const auto reg=(ptr.value>>5)&7u;
    if(data.access_width_bits==16u && data.value<=0xffffu) {
        // These are the specific voice-registers independently read out of
        // the original installed SBAWE.VXD; not a generic VXD driver emulator.
        return ((reg==0u || reg==1u) && data.port==0xE20u) || // IP / IFATN
               (reg==5u && data.port==0xA20u);   // DATA1: DCYSUSV
    }
    if(data.access_width_bits==32u)
        return (reg==3u || reg==6u || reg==7u) &&
               data.port==0x620u; // VTFT / PSST / CSL
    return false;
}
}
bool applyOriginalCreativeVxdVoicePorts(
    awe64::Chip& chip,
    const std::span<const OriginalWin95VxdPortWrite> commands) noexcept {
    if(!chip.ready() || commands.empty() || commands.size()%2u!=0u ||
       commands.size()>64u)return false;
    for(std::size_t i=0;i<commands.size();i+=2)
        if(!validPair(commands[i],commands[i+1]))return false;
    // No allocation, file I/O or manual DSP math in this bridge.
    // Original fork emu8k_outb(E22) calls its emu8k_outw with the same
    // zero-extended selector value; the public P001 Chip exposes outw.
    // A DWORD DATA0 write comprises low16 to 620, high16 to 622.
    for(std::size_t i=0;i<commands.size();i+=2) {
        const auto& ptr=commands[i];
        const auto& d=commands[i+1];
        chip.writePort(0xE22u,static_cast<std::uint16_t>(ptr.value));
        if(d.access_width_bits==32u) {
            chip.writePort(0x620u,static_cast<std::uint16_t>(d.value));
            chip.writePort(0x622u,static_cast<std::uint16_t>(d.value>>16));
        } else {
            chip.writePort(d.port,static_cast<std::uint16_t>(d.value));
        }
    }
    return true;
}
} // namespace awe64::creative
