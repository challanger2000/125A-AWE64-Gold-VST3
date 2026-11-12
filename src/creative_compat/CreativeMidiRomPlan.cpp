#include "CreativeMidiRomPlan.h"
namespace awe64::creative {
std::optional<CreativeRomNotePlan> prepareOriginalCreativeRomNote(
    const MidiMessage& event, const MidiState& state,
    std::span<const std::uint8_t> bank, std::uint16_t resolved_bank,
    std::size_t rom_words) noexcept {
    if(event.kind!=MidiKind::NoteOn || event.data2==0 || event.channel>=16 ||
       event.data1>127 || event.data2>127 || rom_words==0)return std::nullopt;
    const auto program=state.channel(event.channel).program;
    const auto selected=resolveCreativeSbkLayers(
        bank,resolved_bank,program,event.data1,event.data2);
    if(!selected)return std::nullopt;
    CreativeRomNotePlan result{};
    result.channel=event.channel;result.program=program;result.key=event.data1;
    result.velocity=event.data2;result.original_sbk_bank=resolved_bank;
    // The executed original Creative Format-2 VXD bank builder emits
    // at most six 148-byte descriptors, even when seven or more zones
    // match. Preserve the larger inventory separately, but do not start
    // a seventh emulated ROM voice or reject the otherwise valid bank.
    result.count=static_cast<std::uint8_t>(
        selected->count<kOriginalCreativeMaxNoteStartDescriptors
            ? selected->count:kOriginalCreativeMaxNoteStartDescriptors);
    for(std::size_t i=0;i<selected->count;++i){
        const auto& metadata=selected->layers[i];
        const auto address=resolveCreative1MgmRomAddress(
            bank,metadata.rom_sample_selector,rom_words);
        if(!address)return std::nullopt;
        result.layers[i].metadata=metadata;
        result.layers[i].address=*address;
    }
    return result;
}
}
