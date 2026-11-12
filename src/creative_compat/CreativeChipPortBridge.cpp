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
        return (reg<=5u && data.port==0xE20u) || // DATA3 IP, IFATN, PEFE, FMMOD, TREMFRQ, FM2FRQ2
               (reg>=4u && reg<=7u && data.port==0xA20u) || // DATA1 ENVVOL, DCYSUSV, ENVVAL, DCYSUS
               ((reg>=4u && reg<=7u) && data.port==0xA22u); // envelopes/LFO
               // DATA1: DCYSUSV; DATA1: DCYSUS (note release)
    }
    if(data.access_width_bits==32u)
        return (reg==0u && data.port==0xA20u) || // CCCA (real ROM cursor)
               (data.port==0x620u); // DATA0: CPF,PTRX,CVCF,VTFT,Z2,Z1,PSST,CSL
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
            chip.writePort(d.port,static_cast<std::uint16_t>(d.value));
            chip.writePort(static_cast<std::uint16_t>(d.port+2u),
                           static_cast<std::uint16_t>(d.value>>16));
        } else {
            chip.writePort(d.port,static_cast<std::uint16_t>(d.value));
        }
    }
    return true;
}
} // namespace awe64::creative
