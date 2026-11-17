#include "OriginalCreativeSf2RuntimeBank.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
namespace awe64::original_vxd {
namespace {
using Binding=OriginalSf2RuntimePresetBinding;
[[nodiscard]] std::uint32_t identity(std::uint16_t slot,std::uint16_t row) noexcept {
    return (static_cast<std::uint32_t>(slot)<<16u)|row;
}
[[nodiscard]] std::uint32_t identity(Binding const& b) noexcept {
    return identity(b.device_bank_slot,b.device_preset_row);
}
[[nodiscard]] bool charge(std::size_t& used,std::size_t n,std::size_t size,
    std::size_t budget) noexcept {
    if(used>budget || (size!=0u && n>(budget-used)/size))return false;
    used+=n*size;return true;
}
}
OriginalCreativeSf2RuntimeBank::OriginalCreativeSf2RuntimeBank() noexcept {
    retained_bytes_=sizeof(*this);
}
Sf2RuntimeBankStatus OriginalCreativeSf2RuntimeBank::prepareNonRealtime(
    std::span<const OriginalProgramBankSlot> original_slots,
    std::span<const OriginalSf2RuntimeFile> source_files,
    std::span<const Binding> original_bindings,
    OriginalSf2RuntimeResources const& resources,
    std::size_t budget,bool attested) noexcept {
    using Status=Sf2RuntimeBankStatus;
    if(ready_ || leased_)return Status::AlreadyPrepared;
    if(!attested || !resources.original_device_state_verified ||
       !resources.original_volume_tables_verified ||
       !resources.original_format1_verified)return Status::Unverified;
    if(original_slots.size()!=original_program_bank_slots || source_files.empty() ||
       original_bindings.empty())return Status::Malformed;
    std::size_t used=sizeof(*this);
    if(!charge(used,source_files.size(),sizeof(OwnedFile),budget) ||
       !charge(used,original_bindings.size(),sizeof(Binding),budget))return Status::BudgetExceeded;
    std::size_t presets=0u;
    for(const auto& slot:original_slots) {
        if(slot.rows.size()>65536u)return Status::Malformed;
        if(!slot.rows.empty()) {
            if(slot.rows.back().original_source_marker!=0xffffu)return Status::Malformed;
            for(std::size_t i=0;i+1u<slot.rows.size();++i)
                if(slot.rows[i].original_source_marker==0xffffu)return Status::Malformed;
        }
        if(!charge(used,slot.rows.size(),sizeof(OriginalPresetTableRow),budget))return Status::BudgetExceeded;
        for(const auto& row:slot.rows)
            if(row.original_source_marker!=0u && row.original_source_marker!=0xffffu)++presets;
    }
    for(const auto& file:source_files) {
        if(!file.tables.original_generator_templates_verified ||
           !file.tables.original_pitch_tables_verified)return Status::Unverified;
        if(!charge(used,file.bytes.size(),1u,budget))return Status::BudgetExceeded;
        if(file.tables.original_sample_ram_placement_verified &&
           file.tables.ram_base_word>=0x200000u) {
            const auto pool=source_sf2_raw_sample_pool(file.bytes);
            if(!pool || pool->empty() || file.tables.ram_base_word<0x20000eu ||
               file.tables.ram_base_word>0xfffffeu ||
               pool->size()/2u>0xfffffeu-file.tables.ram_base_word)return Status::Malformed;
        }
    }
    for(const auto& binding:original_bindings) {
        if(!binding.original_binding_verified)return Status::Unverified;
        if(binding.device_bank_slot>=original_slots.size() ||
           binding.device_preset_row>=original_slots[binding.device_bank_slot].rows.size() ||
           binding.file_index>=source_files.size())return Status::Malformed;
        const auto& row=original_slots[binding.device_bank_slot].rows[binding.device_preset_row];
        if(row.original_source_marker==0u || row.original_source_marker==0xffffu)return Status::Malformed;
        const auto source=source_sf2_preset_identity(
            source_files[binding.file_index].bytes,binding.source_preset_row);
        if(!source || source->bank!=binding.source_bank ||
           source->program!=binding.source_program)return Status::Malformed;
    }
    // Transactional ownership: no borrowed spans or ready flag are published
    // until all copies, uniqueness checks and retained-capacity checks pass.
    try {
        std::array<std::vector<OriginalPresetTableRow>,original_program_bank_slots> rows{};
        std::vector<OwnedFile> files(source_files.size());
        std::vector<Binding> bindings(original_bindings.begin(),original_bindings.end());
        // Distinct uploaded file allocations cannot occupy the same sample
        // RAM. Aliases of one genuine source allocation share one file index.
        struct RamRange {std::uint32_t begin;std::size_t end;};
        std::vector<RamRange> ranges;
        ranges.reserve(source_files.size());
        for(const auto& file:source_files)
            if(file.tables.original_sample_ram_placement_verified &&
               file.tables.ram_base_word>=0x200000u) {
                const auto pool=source_sf2_raw_sample_pool(file.bytes);
                // The original uploader reserves 14 leading and two
                // trailing words in addition to the source PCM pool.
                ranges.push_back({file.tables.ram_base_word-14u,
                    static_cast<std::size_t>(file.tables.ram_base_word)+pool->size()/2u+2u});
            }
        std::sort(ranges.begin(),ranges.end(),[](auto const& a,auto const& b) noexcept {
            return a.begin<b.begin;
        });
        for(std::size_t i=1u;i<ranges.size();++i)
            if(ranges[i].begin<ranges[i-1u].end)return Status::Malformed;
        std::sort(bindings.begin(),bindings.end(),[](auto const& a,auto const& b) noexcept {
            return identity(a)<identity(b);
        });
        for(std::size_t i=1;i<bindings.size();++i)
            if(identity(bindings[i-1u])==identity(bindings[i]))return Status::Malformed;
        for(std::size_t i=0;i<rows.size();++i)
            rows[i].assign(original_slots[i].rows.begin(),original_slots[i].rows.end());
        for(std::size_t i=0;i<files.size();++i) {
            files[i].bytes.assign(source_files[i].bytes.begin(),source_files[i].bytes.end());
            files[i].tables=source_files[i].tables;
        }
        used=sizeof(*this);
        if(!charge(used,files.capacity(),sizeof(OwnedFile),budget) ||
           !charge(used,bindings.capacity(),sizeof(Binding),budget))return Status::BudgetExceeded;
        for(const auto& r:rows)
            if(!charge(used,r.capacity(),sizeof(OriginalPresetTableRow),budget))return Status::BudgetExceeded;
        for(const auto& f:files)
            if(!charge(used,f.bytes.capacity(),1u,budget))return Status::BudgetExceeded;
        rows_.swap(rows);files_.swap(files);bindings_.swap(bindings);
        resources_=resources;
        for(std::size_t i=0;i<slots_.size();++i)slots_[i].rows=rows_[i];
        retained_bytes_=used;unbound_presets_=presets-bindings_.size();ready_=true;
    }catch(const std::bad_alloc&) {return Status::AllocationFailed;}
    catch(const std::length_error&) {return Status::BudgetExceeded;}
    return Status::Ok;
}
bool OriginalCreativeSf2RuntimeBank::resetNonRealtime() noexcept {
    if(leased_)return false;
    ready_=false;cache_view_={};cache_={};note_status_=Sf2BuildStatus::Unverified;
    for(std::size_t i=0;i<rows_.size();++i) {
        slots_[i].rows={};std::vector<OriginalPresetTableRow>{}.swap(rows_[i]);
    }
    std::vector<OwnedFile>{}.swap(files_);std::vector<Binding>{}.swap(bindings_);
    resources_={};retained_bytes_=sizeof(*this);unbound_presets_=0u;
    // Do not recycle generations when the same owner is prepared again.
    return true;
}
bool OriginalCreativeSf2RuntimeBank::acquireRuntimeLeaseNonRealtime() noexcept {
    if(!ready_ || leased_)return false;
    leased_=true;return true;
}
MidiEngine::VolumeSource OriginalCreativeSf2RuntimeBank::controllerResources() const noexcept {
    if(!ready_)return {};
    return {resources_.original_volume_tables_verified,resources_.cc7.data(),
        resources_.cc11.data(),resources_.device_additive_attenuation,
        resources_.original_device_state_verified,resources_.original_format1_verified,
        resources_.original_format1};
}
SoundFontSource OriginalCreativeSf2RuntimeBank::source() noexcept {
    return ready_?SoundFontSource{this,resolve,compile,selectProgram}:SoundFontSource{};
}
bool OriginalCreativeSf2RuntimeBank::selectProgram(void* opaque,MidiChannel const& c,
    std::uint8_t program,OriginalProgramSelection& selection) noexcept {
    if(!opaque || program>=128u)return false;
    auto& bank=*static_cast<OriginalCreativeSf2RuntimeBank*>(opaque);
    if(!bank.ready_)return false;
    selection=c.original_channel_is_percussion?
        select_original_percussion_program(bank.programMap(),c.bank_msb,program):
        select_original_melodic_program(bank.programMap(),c.bank_msb,program);
    return selection.status==ProgramSelectionStatus::Found;
}
bool OriginalCreativeSf2RuntimeBank::resolve(void* opaque,MidiChannel const& c,
    std::uint8_t key,std::uint8_t velocity,
    std::array<SourceVoice,max_source_note_layers>& voices,std::uint8_t& count) noexcept {
    voices={};count=0u;
    if(!opaque)return false;
    auto& bank=*static_cast<OriginalCreativeSf2RuntimeBank*>(opaque);
    bank.cache_view_={};bank.cache_={};bank.note_status_=Sf2BuildStatus::Unverified;
    if(bank.generation_==std::numeric_limits<std::uint64_t>::max())return false;
    ++bank.generation_;
    if(!bank.ready_ || key>=128u || velocity==0u || velocity>=128u ||
       !c.original_program_selection_valid ||
       !c.original_fine_tune_verified || !c.original_coarse_tune_verified ||
       !c.original_modulation_byte_44d_verified)return false;
    const auto id=identity(c.original_selected_bank_slot,c.original_selected_preset_row);
    const auto at=std::lower_bound(bank.bindings_.begin(),bank.bindings_.end(),id,
        [](auto const& binding,std::uint32_t value) noexcept {return identity(binding)<value;});
    if(at==bank.bindings_.end() || identity(*at)!=id)return false;
    const auto& row=bank.rows_[at->device_bank_slot][at->device_preset_row];
    if(row.original_bank_byte!=c.original_selected_bank ||
       row.original_program_byte!=c.original_selected_program)return false;
    const auto& file=bank.files_[at->file_index];
    auto selected=select_sf2_raw_layers_by_preset(file.bytes,at->source_preset_row,key,velocity);
    if(selected.status!=Sf2ReadStatus::Ok || selected.count==0u) {
        bank.note_status_=selected.status==Sf2ReadStatus::TooManyLayers?
            Sf2BuildStatus::TooManyLayers:Sf2BuildStatus::Unsupported;
        return false;
    }
    OriginalNoteVolumeResources volume{};
    volume.original_velocity_table_verified=bank.resources_.original_volume_tables_verified;
    volume.velocity_table=bank.resources_.velocity;volume.controllers=bank.controllerResources();
    std::array<OriginalSf2SourceChannelState,sf2_max_selected_layers> contexts{};
    for(std::size_t i=0;i<selected.count;++i) {
        auto& layer=selected.layers[i];
        // Logical MIDI/VXD identity comes ONLY from the latched original
        // row; source PHDR identity was independently checked on import.
        layer.bank=c.original_selected_bank;layer.program=c.original_selected_program;
        auto& context=contexts[i];
        context.original_note_context_verified=true;
        context.original_note_volume_source=&volume;
        context.original_channel_volume=c.volume;context.original_channel_expression=c.expression;
        context.original_channel_pan=c.pan;context.original_channel_chorus=c.chorus;
        context.original_channel_reverb=c.reverb;
        context.original_device_effects_flag=bank.resources_.device_effects_mode;
        context.original_program_identity_verified=true;
        context.original_program_bank_slot=at->device_bank_slot;
        context.original_program_preset_row=at->device_preset_row;
        context.original_source_preset_binding_verified=true;
        context.original_source_preset_row=at->source_preset_row;
        context.original_descriptor_group=at->original_descriptor_group;
        auto& state=context.original_voice_start;
        state.original_descriptor_verified=true;
        state.original_device_state_verified=bank.resources_.original_device_state_verified;
        state.original_sample_placement_verified=file.tables.original_sample_ram_placement_verified;
        // ROM placement is directly bounded by the original descriptor
        // compiler; RAM must have a separately authenticated upload base.
        state.original_ram_sample=(layer.sample.sample_type&0x8000u)==0u;
        if(!state.original_ram_sample)state.original_sample_placement_verified=true;
        state.original_channel_tune_1_cents=c.original_fine_tune_cents;
        state.original_channel_tune_2_cents=c.original_coarse_tune_cents;
        state.original_lfo1_pitch_device_0a=c.original_modulation_bucket;
        state.original_lfo1_pitch_device_0b=c.original_modulation_byte_44d;
        state.original_derive_pitch_target_from_channel=true;
        state.original_channel_pitch_tune=c.original_pitch_tune;
        state.original_channel_bend_delta=c.original_bend_delta;
        state.original_derive_volume_target_from_attenuation=true;
        state.original_volume_target_tables_verified=bank.resources_.original_volume_tables_verified;
        state.original_volume_positive16=bank.resources_.volume_positive16;
        state.original_volume_negative15=bank.resources_.volume_negative15;
    }
    bank.cache_=prepare_original_sf2_zones(selected,file.tables,
        {contexts.data(),selected.count});
    bank.note_status_=bank.cache_.status;
    if(bank.cache_.status!=Sf2BuildStatus::Ok)return false;
    for(std::size_t i=0;i<bank.cache_.count;++i)
        bank.cache_.zones[i].voice.original_source_generation=bank.generation_;
    bank.cached_key_=key;
    bank.cache_view_={std::span<const VerifiedSf2Zone>(bank.cache_.zones.data(),bank.cache_.count),
                      false,bank.programMap()};
    auto source=original_verified_sf2_bank_source(bank.cache_view_);
    return source.resolve(source.opaque,c,key,velocity,voices,count);
}
bool OriginalCreativeSf2RuntimeBank::compile(void* opaque,SourceVoice const& voice,
    std::uint8_t physical,std::uint8_t key,OriginalVoiceProgram& program) noexcept {
    if(!opaque)return false;
    auto& bank=*static_cast<OriginalCreativeSf2RuntimeBank*>(opaque);
    if(!bank.ready_ || bank.cache_.status!=Sf2BuildStatus::Ok || key!=bank.cached_key_ ||
       voice.original_source_generation!=bank.generation_)return false;
    auto source=original_verified_sf2_bank_source(bank.cache_view_);
    return source.compile(source.opaque,voice,physical,key,program);
}
} // namespace awe64::original_vxd
