#include "OriginalCreativeVerifiedSf2Bank.hpp"
#include "OriginalCreativeProgramSelection.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>

namespace awe64::original_vxd {
namespace {
[[nodiscard]] bool resolve(void* opaque,MidiChannel const& channel,
    std::uint8_t key,std::uint8_t velocity,
    std::array<SourceVoice,max_source_note_layers>& voices,
    std::uint8_t& count) noexcept {
    if(!opaque || key>=128u || velocity==0u || velocity>=128u)
        return false;
    const auto& bank=*static_cast<VerifiedSf2Bank const*>(opaque);
    count=0u;
    if(bank.zones.size()>std::numeric_limits<std::uint16_t>::max())
        return false;
    std::uint8_t selected_bank=channel.bank_msb;
    std::uint8_t selected_program=channel.program;
    if(bank.original_program_map.authentic_driver_records_verified) {
        // Program Change already ran +0x6D05. It is NOT re-run on
        // NoteOn, even if MIDI CC0 changed the requested bank meanwhile.
        if(!channel.original_program_selection_valid)return false;
        selected_bank=channel.original_selected_bank;
        selected_program=channel.original_selected_program;
    }else if(bank.original_melodic_program_fallback_verified &&
             !channel.original_channel_is_percussion) {
        // Original LCOD+0x6D05 resolves the bank at PROGRAM CHANGE.
        // CC0 merely changes the requested bank. Re-evaluating the
        // directory on NoteOn would switch an already-loaded preset
        // without a new C0 event, even when the original had latched it.
        if(!channel.original_program_selection_valid)return false;
        selected_bank=channel.original_selected_bank;
        selected_program=channel.original_selected_program;
    }
    for(std::size_t i=0;i<bank.zones.size();++i) {
        const auto& zone=bank.zones[i];
        if(zone.bank!=selected_bank || zone.program!=selected_program ||
           key<zone.key_low || key>zone.key_high ||
           velocity<zone.velocity_low || velocity>zone.velocity_high)
            continue;
        if(bank.original_program_map.authentic_driver_records_verified &&
           (!zone.original_preset_identity_verified ||
            zone.original_bank_slot!=channel.original_selected_bank_slot ||
            zone.original_preset_row!=channel.original_selected_preset_row))
            continue;
        if(!zone.original_zone_verified || !zone.voice.source_verified ||
           !zone.voice.note_start_descriptor_verified ||
           !zone.original_state.original_descriptor_verified ||
           !zone.original_state.original_device_state_verified ||
           !zone.original_state.original_sample_placement_verified ||
           count>=max_source_note_layers)
            return false; // fail closed; never truncate a layered note
        // Pure original descriptor compiler: validate every matching zone
        // BEFORE the first live voice allocation or EMU8000 port write.
        // The verified main DATA values are independent of the physical
        // index (the MIDI engine inserts the voice into its selectors).
        // A malformed second SF2 layer must never leave the first layer
        // half-allocated with the MIDI engine fault latched.
        SourceVoice chosen=zone.voice;
        chosen.source_slot=static_cast<std::uint16_t>(i);
        chosen.original_fine_tune_override=channel.original_fine_tune_verified;
        chosen.original_coarse_tune_override=channel.original_coarse_tune_verified;
        chosen.original_fine_tune_cents=channel.original_fine_tune_cents;
        chosen.original_coarse_tune_cents=channel.original_coarse_tune_cents;
        auto source=zone.original_state;
        if(chosen.original_fine_tune_override)
            source.original_channel_tune_1_cents=chosen.original_fine_tune_cents;
        if(chosen.original_coarse_tune_override)
            source.original_channel_tune_2_cents=chosen.original_coarse_tune_cents;
        if(source.original_derive_pitch_target_from_channel) {
            chosen.original_live_pitch_state_verified=true;
            chosen.original_live_pitch_tune=channel.original_pitch_tune;
            chosen.original_live_bend_delta=channel.original_bend_delta;
            source.original_channel_pitch_tune=channel.original_pitch_tune;
            source.original_channel_bend_delta=channel.original_bend_delta;
            const auto baseline=original_sf2_initial_pitch(
                chosen.note_start_descriptor,key,
                source.original_channel_tune_1_cents,
                source.original_channel_tune_2_cents);
            if(!baseline)return false;
            chosen.original_pitch_base=*baseline;
        }
        OriginalVoiceProgram preflight{};
        if(!compile_original_sf2_main(chosen,0u,key,source,preflight) ||
           !preflight.source_verified)
            return false;
        voices[count++]=chosen;
    }
    return count!=0u;
}

[[nodiscard]] bool compile(void* opaque,SourceVoice const& selected,
    std::uint8_t physical,std::uint8_t key,
    OriginalVoiceProgram& program) noexcept {
    if(!opaque || selected.source_slot==0xffffu)return false;
    const auto& bank=*static_cast<VerifiedSf2Bank const*>(opaque);
    if(selected.source_slot>=bank.zones.size())return false;
    const auto& zone=bank.zones[selected.source_slot];
    if(!zone.original_zone_verified ||
       !zone.voice.source_verified ||
       selected.descriptor!=zone.voice.descriptor ||
       !selected.note_start_descriptor_verified ||
       selected.note_start_descriptor!=zone.voice.note_start_descriptor ||
       selected.release_end!=zone.voice.release_end ||
       (!zone.original_state.original_derive_pitch_target_from_channel &&
        selected.original_pitch_base!=zone.voice.original_pitch_base) ||
       selected.original_pitch_base_verified!=zone.voice.original_pitch_base_verified)
        return false;
    auto source=zone.original_state;
    if(selected.original_fine_tune_override)
        source.original_channel_tune_1_cents=selected.original_fine_tune_cents;
    if(selected.original_coarse_tune_override)
        source.original_channel_tune_2_cents=selected.original_coarse_tune_cents;
    if(source.original_derive_pitch_target_from_channel) {
        if(!selected.original_live_pitch_state_verified)return false;
        source.original_channel_pitch_tune=selected.original_live_pitch_tune;
        source.original_channel_bend_delta=selected.original_live_bend_delta;
        const auto baseline=original_sf2_initial_pitch(
            selected.note_start_descriptor,key,
            source.original_channel_tune_1_cents,
            source.original_channel_tune_2_cents);
        if(!baseline || selected.original_pitch_base!=*baseline)return false;
    }
    return compile_original_sf2_main(selected,physical,key,source,program);
}
[[nodiscard]] bool select_program(void* opaque, MidiChannel const& channel,
    std::uint8_t program,OriginalProgramSelection& result) noexcept {
    if(!opaque || program>=128u)return false;
    const auto& bank=*static_cast<VerifiedSf2Bank const*>(opaque);
    if(bank.original_program_map.authentic_driver_records_verified) {
        result=channel.original_channel_is_percussion
            ?select_original_percussion_program(
                bank.original_program_map,channel.bank_msb,program)
            :select_original_melodic_program(
                bank.original_program_map,channel.bank_msb,program);
        return result.status==ProgramSelectionStatus::Found;
    }
    if(bank.original_melodic_program_fallback_verified &&
       !channel.original_channel_is_percussion) {
        const auto contains=[](const void* opaque,std::uint8_t b,
            std::uint8_t p) noexcept -> bool {
            const auto& dir=*static_cast<VerifiedSf2Bank const*>(opaque);
            for(const auto& zone:dir.zones)
                if(zone.bank==b && zone.program==p)return true;
            return false;
        };
        const auto chosen=original_melodic_program_bank(
            channel.bank_msb,program,opaque,contains);
        if(!chosen.found)return false;
        result.status=ProgramSelectionStatus::Found;
        result.resolved_program=program;
        result.resolved_bank=chosen.selected_bank;
        result.fallback_stage=chosen.fallback_pass;
        return true;
    }
    // Without verified original directory, no authentic bank resolution
    // can be asserted. The previous compatible zone-only path remains
    // available for independently verified earlier local tests.
    result.status=ProgramSelectionStatus::Found;
    result.resolved_bank=channel.bank_msb;
    result.resolved_program=program;
    return true;
}
} // namespace

SoundFontSource original_verified_sf2_bank_source(VerifiedSf2Bank& bank) noexcept {
    return SoundFontSource{&bank,resolve,compile,select_program};
}
} // namespace awe64::original_vxd
