#include "CreativeClockedSf2NotePlan.h"
#include "CreativeChipPortBridge.h"
#include <cstddef>
#include <cstdint>
#include <span>
namespace awe64::creative {
std::optional<ClockedSf2NotePlan> candidateClockedSf2TypedOuts(
    const OriginalSf2FullNoteOnPlan& plan,
    const std::span<const std::uint32_t> frames) noexcept {
    if(plan.voices==0u ||
       plan.voices>kOriginalCreativeMaxNoteStartDescriptors ||
       plan.count!=static_cast<std::size_t>(plan.voices)*
                   kOriginalSf2OneVoiceStartWrites ||
       frames.empty() ||
       frames.size()>SourceClockedPortPlayer::kMaxEvents ||
       frames.front()!=0u)
        return std::nullopt;
    // Validate the original x86 voice allocation prologues and ALL
    // main VXD register pairs before expanding the first native OUT.
    std::uint32_t usedVoices=0u;
    for(std::size_t i=0u;i<plan.voices;++i) {
        const auto at=5u*i;
        const auto& start=plan.out[at];
        if(start.port!=0xe22u || start.access_width_bits!=8u ||
           start.value<0x60u || start.value>0x7du)
            return std::nullopt;
        const auto physical=static_cast<std::uint8_t>(start.value&0x1fu);
        if(physical>=kOriginalCreativeMusicVoices ||
           (usedVoices&(1u<<physical))!=0u)
            return std::nullopt;
        usedVoices|=1u<<physical;
        if(!validateOriginalCreativeVxdVoicePorts(
               std::span<const OriginalWin95VxdPortWrite>(
                   plan.out.data()+at+1u,4u)))
            return std::nullopt;
        const auto main=static_cast<std::size_t>(plan.voices)*5u+
            i*kOriginalSf2OneVoiceMainWrites;
        if(!validateOriginalCreativeVxdVoicePorts(
               std::span<const OriginalWin95VxdPortWrite>(
                   plan.out.data()+main,kOriginalSf2OneVoiceMainWrites)))
            return std::nullopt;
    }
    ClockedSf2NotePlan result{};
    std::size_t out=0u;
    auto push=[&](const std::uint16_t port,const std::uint16_t value) noexcept {
        if(out>=frames.size() || out>=result.words.size())return false;
        const auto clock=frames[out];
        if(clock>44100u*30u ||
           (out>0u && clock<frames[out-1u]))return false;
        result.words[out++]={clock,port,value};
        return true;
    };
    for(std::size_t i=0u;i<plan.count;++i) {
        const auto& command=plan.out[i];
        if(command.access_width_bits==8u) {
            if(command.port!=0xe22u || command.value>0xffu ||
               !push(0xe22u,static_cast<std::uint16_t>(command.value)))
                return std::nullopt;
        }else if(command.access_width_bits==16u) {
            if(command.value>0xffffu ||
               !push(command.port,static_cast<std::uint16_t>(command.value)))
                return std::nullopt;
        }else if(command.access_width_bits==32u) {
            if(!push(command.port,static_cast<std::uint16_t>(command.value)) ||
               !push(static_cast<std::uint16_t>(command.port+2u),
                     static_cast<std::uint16_t>(command.value>>16u)))
                return std::nullopt;
        }else return std::nullopt;
    }
    if(out!=frames.size())return std::nullopt;
    result.count=out;
    return result;
}
} // namespace awe64::creative
