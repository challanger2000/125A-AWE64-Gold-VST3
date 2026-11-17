#include "OriginalCreativeSf2ResourceBuilder.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace awe64::original_vxd {
namespace {
[[nodiscard]] std::int32_t signed_word(std::uint16_t w) noexcept {
    return w<=0x7fffu?static_cast<std::int32_t>(w):
        static_cast<std::int32_t>(w)-65536;
}
[[nodiscard]] std::int16_t signed_16(std::int32_t n) noexcept {
    const auto w=static_cast<std::uint16_t>(n);
    return static_cast<std::int16_t>(signed_word(w));
}
[[nodiscard]] std::int16_t convert_instrument(unsigned op,std::int16_t raw) noexcept {
    const auto x=static_cast<std::int32_t>(raw);
    std::int32_t y=x;
    switch(op){
        case 5u:case 6u:case 7u:y=(x*6991+32768)/65536;break;
        case 8u:y=(x+15)/29-153;break;
        case 9u:y=x/12;break;
        case 10u:y=(x*2329+32768)/65536;break;
        case 11u:y=(x*1165+32768)/65536;break;
        case 13u:y=x*128/120;break;
        case 29u:case 37u:y=127+(-127*x)/1000;break;
        case 51u:y=x*100;break;
        default:break;
    }
    return signed_16(y);
}
[[nodiscard]] std::int16_t convert_preset(unsigned op,std::int16_t raw) noexcept {
    const auto v=convert_instrument(op,raw);
    switch(op) {
        case 5u:case 6u:case 7u:case 8u:case 9u:case 10u:
        case 11u:case 13u:case 29u:case 37u:case 46u:case 47u:
        case 49u:case 51u:case 52u:case 56u:case 58u:
            return signed_16(static_cast<std::int32_t>(v)-convert_instrument(op,0));
        default:return v;
    }
}
[[nodiscard]] std::int32_t get(Descriptor const& d,unsigned op) noexcept {
    const auto v=static_cast<std::uint16_t>(
        static_cast<unsigned>(d[2u*op])|
        (static_cast<unsigned>(d[2u*op+1u])<<8u));
    return signed_word(v);
}
void set(Descriptor& d,unsigned op,std::uint16_t v) noexcept {
    d[2u*op]=static_cast<std::uint8_t>(v);
    d[2u*op+1u]=static_cast<std::uint8_t>(v>>8u);
}
void set_dword(Descriptor& d,unsigned at,std::uint32_t v) noexcept {
    for(unsigned i=0;i<4u;++i)d[at+i]=static_cast<std::uint8_t>(v>>(8u*i));
}
[[nodiscard]] std::uint16_t clamp_word(std::int32_t x,std::int32_t lo,std::int32_t hi) noexcept {
    return static_cast<std::uint16_t>(std::clamp(x,lo,hi));
}
[[nodiscard]] bool pitch_of(std::uint32_t rate,OriginalSf2SourceTables const& table,
    std::uint32_t& result) noexcept {
    if(rate==0u)return false;
    auto n=static_cast<std::uint32_t>(static_cast<std::uint64_t>(rate)*0x2f8eull);
    if(n==0u)return false;
    unsigned shifts=0;
    while((n&0x80000000u)==0u){n<<=1u;++shifts;}
    const auto index=(n>>24u)&0x7fu;
    const auto fraction=(n>>17u)&0x7fu;
    const auto exponent=(31u-shifts-15u)<<20u;
    const auto slope=static_cast<std::int32_t>(table.pitch_slope[index]);
    result=exponent+table.pitch_base[index]+
        static_cast<std::uint32_t>(slope*static_cast<std::int32_t>(fraction));
    return result<=0x00ffffffu;
}
[[nodiscard]] std::uint8_t attack_rate(std::int32_t x) noexcept {
    if(x<-599){const auto value=(37-x)/75+8;return static_cast<std::uint8_t>(std::min(128,value));}
    if(x>=4300)return 1u;
    const auto q=(x+500)/150;
    return static_cast<std::uint8_t>(std::max(1,((~q&7)|8)>>((q/8)&31)));
}
[[nodiscard]] std::uint32_t pcod_exp(std::int32_t x,std::int32_t bias) noexcept {
    const auto product=(static_cast<std::int64_t>(x)+bias)*65536ll;
    const auto low=static_cast<std::uint32_t>(product);
    const auto signed_p=static_cast<std::int64_t>(low)-
        (low>=0x80000000u?4294967296ll:0ll);
    const auto q=static_cast<std::int32_t>(signed_p/1200ll);
    const auto shift=(static_cast<std::uint32_t>(16-q/65536))&31u;
    return (65536u+(static_cast<std::uint32_t>(q)&65535u))>>shift;
}
[[nodiscard]] std::uint16_t env(unsigned op,std::int32_t x) noexcept {
    switch(op){
        case 21u:case 23u:case 25u:case 33u:
            if(x<-11999)return 32768u;
            if(x>=5484)return 0u;
            return static_cast<std::uint16_t>(std::max(0,32768-
                static_cast<std::int32_t>(pcod_exp(x,12516))));
        case 22u:case 24u:
            if(x<-15999)return 0u;
            return static_cast<std::uint16_t>(std::min(255u,pcod_exp(x,9126)));
        case 26u:case 34u:return attack_rate(x);
        case 27u:case 35u:
            if(x<=-5369)return 127u;
            return static_cast<std::uint16_t>(std::max(0,127-
                static_cast<std::int32_t>(pcod_exp(x,4130))));
        case 28u:case 30u:case 36u:case 38u:
            if(x<=1800)return static_cast<std::uint16_t>(std::min(127,(37-x)/75+39));
            {const auto q=(x-1800)/150;return static_cast<std::uint16_t>(((~q&7)|8)>>((q/8)&31));}
        case 29u:case 37u:return clamp_word(x,0,127);
        default:return 0u;
    }
}
[[nodiscard]] bool make_descriptors(Sf2SelectedLayer const& s,
    OriginalSf2SourceTables const& r,OriginalSf2SourceChannelState const& ctx,
    Descriptor& before,Descriptor& after) noexcept {
    if(s.bank>255u || !ctx.original_note_context_verified ||
       (!ctx.original_attenuation_verified && !ctx.original_note_volume_source) ||
       ctx.original_channel_pan>127u || ctx.original_channel_chorus>127u ||
       ctx.original_channel_reverb>127u)return false;
    if((s.sample.sample_type&0x7fffu)!=1u &&
       !r.original_stereo_descriptor_conversion_verified)return false;
    // No SoundFont default/template substitution. The 59-word original
    // template values are authenticated input to this clean-room port.
    for(unsigned op=0u;op<59u;++op) {
        const auto& pg=s.preset_local.has(op)?s.preset_local:s.preset_global;
        const auto& ig=s.instrument_local.has(op)?s.instrument_local:s.instrument_global;
        if(op==43u || op==44u){
            set(before,op,static_cast<std::uint16_t>(
                std::clamp(static_cast<std::int32_t>(r.preset_template[op])+
                           r.instrument_template[op],-32768,32767)));
            continue;
        }
        const auto p=pg.has(op)?convert_preset(op,signed_16(pg.get(op))):r.preset_template[op];
        const auto i=ig.has(op)?convert_instrument(op,signed_16(ig.get(op))):r.instrument_template[op];
        set(before,op,static_cast<std::uint16_t>(std::clamp(
           static_cast<std::int32_t>(i)+((op==46u||op==47u)?0:p),-32768,32767)));
    }
    set(before,46u,s.key);set(before,47u,s.velocity);set(before,53u,s.sample_id);
    const auto ram=(s.sample.sample_type&0x8000u)==0u;
    std::uint32_t base=0u;
    if(ram) {
        if(!r.original_sample_ram_placement_verified ||
           r.ram_base_word<0x200000u ||
           static_cast<std::uint64_t>(r.ram_base_word)+s.sample.end>0x1000000u)
            return false;
        base=r.ram_base_word;
    }else if(s.sample.end>0x80000u)return false;
    const auto start=base+s.sample.start,end=base+s.sample.end;
    const auto ls=base+s.sample.loop_start,le=base+s.sample.loop_end;
    // Preserve the genuine SoundFont's +2 loop-end guard words. Creative
    // LCOD+0x5B90/+0x5BA0 reads loopStart/loopEnd only for mode bit0;
    // LCOD+0x5BC8 derives end+4/end+8 for nonlooping samples. No clip to
    // sample.end: that would change the original's loop register writes.
    if(start<4u || end<=start || ls<start || ls>=le ||
       (le>end && le-end>2u) ||
       (ram && le>0x1000000u) || (!ram && le>0x80000u))
        return false;
    std::uint32_t pitch=0u;
    if(!pitch_of(s.sample.sample_rate,r,pitch))return false;
    set_dword(before,0x76u,start);set_dword(before,0x7au,end);
    set_dword(before,0x7eu,ls);set_dword(before,0x82u,le);
    set_dword(before,0x86u,pitch);
    before[0x8au]=s.sample.original_pitch;
    before[0x8bu]=static_cast<std::uint8_t>(s.sample.correction);
    set(before,0x8cu/2u,s.sample.sample_type);
    set(before,0x8eu/2u,s.sample.sample_link);
    set_dword(before,0x90u,s.preset_row);
    after=before;
    set(after,0x8cu/2u,1u);
    if(ctx.original_note_volume_source) {
        const auto attenuation=original_note_attenuation(
            *ctx.original_note_volume_source,s.velocity,
            static_cast<std::int16_t>(get(before,48u)),
            ctx.original_channel_volume,ctx.original_channel_expression,!ram);
        if(!attenuation || (ctx.original_attenuation_verified &&
           ctx.original_encoded_attenuation!=*attenuation))return false;
        set(after,19u,*attenuation);
    }else set(after,19u,ctx.original_encoded_attenuation);
    const auto attack=attack_rate(get(before,34u));
    const auto cutoff=static_cast<std::int32_t>(clamp_word(get(before,8u),0,255));
    const auto actual_cutoff=(s.bank<128u && attack<125u)?
        (static_cast<std::int32_t>(std::max(70u,static_cast<unsigned>(s.velocity)))*
         cutoff+160)/128:cutoff;
    set(after,8u,static_cast<std::uint16_t>(actual_cutoff));
    set(after,9u,clamp_word(get(before,9u),0,15));
    for(auto op:{5u,6u,7u,10u,11u,13u})
        set(after,op,clamp_word(get(before,op),-128,127));
    std::array<std::int32_t,59> fields{};
    for(unsigned op=0u;op<59u;++op)fields[op]=get(before,op);
    for(auto entry:std::array<std::array<unsigned,2>,4>{{
           {{27u,31u}},{{28u,32u}},{{35u,39u}},{{36u,40u}}}}) {
        const auto value=fields[entry[0]]+
            (60-static_cast<std::int32_t>(s.key))*fields[entry[1]];
        if(s.bank<128u)fields[entry[0]]=std::clamp(value,-32768,32767);
    }
    for(const auto op:std::array<unsigned,14>{
        26u,34u,21u,23u,25u,33u,28u,36u,30u,38u,27u,35u,22u,24u})
        set(after,op,env(op,fields[op]));
    for(const auto op:{29u,37u})set(after,op,clamp_word(fields[op],0,127));
    const auto record_pan=std::clamp(get(before,17u)*16/125+64,0,127);
    const auto pan_coord=383-2*(static_cast<std::int32_t>(ctx.original_channel_pan)+record_pan);
    const auto pan=static_cast<std::uint8_t>(
        pan_coord>=254?255:pan_coord<2?0:pan_coord);
    set(after,17u,pan);set(after,18u,pan==0u?255u:static_cast<std::uint8_t>(-pan));
    for(const auto op:{15u,16u}) {
        auto send=get(before,op);
        if(ctx.original_device_effects_flag==0u)send=send*100/392;
        const auto channel=op==15u?ctx.original_channel_chorus:ctx.original_channel_reverb;
        set(after,op,clamp_word(send+channel,0,255));
    }
    return true;
}
}
bool bind_original_creative_uploaded_ram_bank(
    OriginalSf2SourceTables& r,
    const OriginalCreativeRamUploadResult& uploaded) noexcept {
    const auto& placement=uploaded.placement;
    const bool verified=uploaded.status==OriginalSf2UploadStatus::Ok &&
        uploaded.source_pcm_words!=0u &&
        placement.allocation_start_word>=0x200000u &&
        placement.first_sample_word==placement.allocation_start_word+14u &&
        static_cast<std::uint64_t>(placement.allocation_start_word)+
            uploaded.source_pcm_words+16u==placement.next_bank_word &&
        placement.next_bank_word<=0x1000000u;
    if(!verified) {
        r.original_sample_ram_placement_verified=false;
        r.ram_base_word=0u;
        return false;
    }
    r.original_sample_ram_placement_verified=true;
    r.ram_base_word=placement.first_sample_word;
    return true;
}
OriginalSf2PreparedBank prepare_original_sf2_zones(
    Sf2LayerSelection const& selected,
    OriginalSf2SourceTables const& original_tables,
    std::span<const OriginalSf2SourceChannelState> states) noexcept {
    OriginalSf2PreparedBank result{};
    if(selected.status!=Sf2ReadStatus::Ok || selected.count==0u ||
       selected.count>sf2_max_selected_layers || states.size()!=selected.count){
        result.status=Sf2BuildStatus::Malformed;return result;
    }
    if(!original_tables.original_generator_templates_verified ||
       !original_tables.original_pitch_tables_verified){
        result.status=Sf2BuildStatus::Unverified;return result;
    }
    for(std::size_t i=0;i<selected.count;++i) {
        const auto& s=selected.layers[i];const auto& c=states[i];
        if(s.preset_global_mod.count || s.preset_local_mod.count ||
           s.instrument_global_mod.count || s.instrument_local_mod.count){
            // The original 1996 modulator transformation is still unknown.
            // Keep structural evidence but never claim native register parity.
            result.status=Sf2BuildStatus::Unsupported;return result;
        }
        if(!c.original_program_identity_verified ||
           c.original_program_preset_row!=s.preset_row ||
           (!c.original_voice_pitch_base_verified &&
            !c.original_voice_start.original_derive_pitch_target_from_channel) ||
           !c.original_voice_start.original_descriptor_verified ||
           !c.original_voice_start.original_device_state_verified ||
           !c.original_voice_start.original_sample_placement_verified ||
           !c.original_note_context_verified ||
           (!c.original_attenuation_verified && !c.original_note_volume_source)) {
            result.status=Sf2BuildStatus::Unverified;return result;
        }
        auto& out=result.zones[i];
        Descriptor original{},started{};
        if(!make_descriptors(s,original_tables,c,original,started)) {
            result.status=Sf2BuildStatus::Unsupported;return result;
        }
        out.bank=static_cast<std::uint8_t>(s.bank);
        out.program=s.program;out.key_low=s.key;out.key_high=s.key;
        out.velocity_low=s.velocity;out.velocity_high=s.velocity;
        out.original_bank_slot=c.original_program_bank_slot;
        out.original_preset_row=c.original_program_preset_row;
        out.original_preset_identity_verified=true;
        auto& v=out.voice;
        v.descriptor=original; // stereo source class 2/4 preserved
        // The engine expects stereo worklist SAMPLE TYPE WITHOUT ROM bit.
        const auto source_type=static_cast<std::uint16_t>(s.sample.sample_type&0x7fffu);
        set(v.descriptor,0x8cu/2u,source_type);
        v.note_start_descriptor=started;
        v.note_start_descriptor_verified=true;
        v.release_end=0u; // No fabricated stored original voice end marker
        v.original_pitch_base=c.original_voice_pitch_base;
        v.original_pitch_base_verified=true;
        if(c.original_voice_start.original_derive_pitch_target_from_channel) {
            const auto baseline=original_sf2_initial_pitch(started,s.key,
                c.original_voice_start.original_channel_tune_1_cents,
                c.original_voice_start.original_channel_tune_2_cents);
            if(!baseline || (c.original_voice_pitch_base_verified &&
               c.original_voice_pitch_base!=*baseline)) {
                result.status=Sf2BuildStatus::Unverified;return result;
            }
            v.original_pitch_base=*baseline;
        }
        v.source_verified=true;
        if(c.original_note_volume_source) {
            // LCOD+0x5A88/+0x5A90 retains the original amplitude and
            // velocity-level terms for subsequent live CC7/CC11 updates.
            v.original_amplitude_word=static_cast<std::int16_t>(get(original,48u));
            v.original_velocity_volume_level=
                c.original_note_volume_source->velocity_table[s.velocity];
            v.volume_terms_verified=true;
        }
        out.original_state=c.original_voice_start;
        if(c.original_voice_start.original_derive_pitch_target_from_channel ||
           c.original_voice_start.original_derive_volume_target_from_attenuation) {
            OriginalVoiceProgram preflight{};
            if(!compile_original_sf2_main(v,0u,s.key,out.original_state,preflight)) {
                result.status=Sf2BuildStatus::Unverified;return result;
            }
        }
        out.original_zone_verified=true;
        ++result.count;
    }
    result.status=Sf2BuildStatus::Ok;
    return result;
}
} // namespace awe64::original_vxd
