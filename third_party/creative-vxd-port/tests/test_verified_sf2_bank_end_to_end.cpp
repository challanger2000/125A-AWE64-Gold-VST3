#include "OriginalCreativeVerifiedSf2Bank.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
using namespace awe64::original_vxd;
#define CHECK(c) do {if(!(c)){std::fprintf(stderr,"E2E_FAIL %s:%d %s\n",__FILE__,__LINE__,#c);std::exit(1);}}while(false)

void word(Descriptor& d,std::size_t off,std::uint16_t v) noexcept {
    d[off]=static_cast<std::uint8_t>(v);
    d[off+1]=static_cast<std::uint8_t>(v>>8u);
}
void dword(Descriptor& d,std::size_t off,std::uint32_t v) noexcept {
    for(unsigned i=0;i<4u;++i)d[off+i]=static_cast<std::uint8_t>(v>>(8u*i));
}

struct RecordedIo {
    struct Event {std::uint16_t port;std::uint32_t val;std::uint8_t sel;char type;};
    std::array<Event,4096> events{};
    std::array<std::uint32_t,32> vtft{},ccca{},psst{},csl{};
    std::array<std::uint16_t,32> sus{};
    std::uint8_t sel=0u;
    std::size_t count=0;
    bool push(std::uint16_t port,std::uint32_t val,char type) noexcept {
        if(count==events.size())return false;
        events[count++]={port,val,sel,type};return true;
    }
    static bool out8(void* p,std::uint16_t port,std::uint8_t val) noexcept {
        auto& b=*static_cast<RecordedIo*>(p);
        if(port!=0xe22u)return false;
        b.sel=val;return b.push(port,val,'b');
    }
    static bool out16(void* p,std::uint16_t port,std::uint16_t val) noexcept {
        auto& b=*static_cast<RecordedIo*>(p);
        if(port==0xa20u && (b.sel&0xe0u)==0xa0u)
            b.sus[b.sel&31u]=val;
        if(port!=0xe20u && port!=0xa20u && port!=0xa22u)return false;
        return b.push(port,val,'w');
    }
    static bool out32(void* p,std::uint16_t port,std::uint32_t val) noexcept {
        auto& b=*static_cast<RecordedIo*>(p);
        if(port==0x620u){
            auto i=b.sel&31u;
            if((b.sel&0xe0u)==0x60u)b.vtft[i]=val;
            if((b.sel&0xe0u)==0xc0u)b.psst[i]=val;
            if((b.sel&0xe0u)==0xe0u)b.csl[i]=val;
        }else if(port==0xa20u && (b.sel&0xe0u)==0u)
            b.ccca[b.sel&31u]=val;
        else if(port!=0x620u && port!=0xa20u)return false;
        return b.push(port,val,'D');
    }
    static bool in16(void* p,std::uint16_t port,std::uint16_t& val) noexcept {
        auto& b=*static_cast<RecordedIo*>(p);
        if(port!=0xa20u || (b.sel&0xe0u)!=0xa0u)return false;
        val=b.sus[b.sel&31u];return b.push(port,val,'r');
    }
    static bool in32(void* p,std::uint16_t port,std::uint32_t& val) noexcept {
        auto& b=*static_cast<RecordedIo*>(p);
        const auto reg=b.sel&0xe0u;
        const auto id=b.sel&31u;
        if(port==0x620u && reg==0x60u)val=b.vtft[id];
        else if(port==0x620u && reg==0xc0u)val=b.psst[id];
        else if(port==0x620u && reg==0xe0u)val=b.csl[id];
        else if(port==0xa20u && reg==0u)val=b.ccca[id];
        else return false;
        return b.push(port,val,'d');
    }
    PortIo io() noexcept {return {this,out8,out16,in16,in32,out32};}
};

// This fixture is *synthetic*, but only uses already verified descriptor
// encodings and documented original C++ fields. No user/private original
// bank bytes are distributed, and no synthetic trace proves bit-parity.
VerifiedSf2Zone mono_zone() {
    VerifiedSf2Zone z{};
    z.original_zone_verified=true;
    z.bank=1;z.program=7;
    z.key_low=60;z.key_high=67;
    z.velocity_low=1;z.velocity_high=127;
    z.voice.source_verified=true;
    z.voice.release_end=0u;
    z.voice.original_pitch_base=0xe000u;
    z.voice.original_pitch_base_verified=true;
    z.voice.note_start_descriptor_verified=true;
    auto& d=z.voice.note_start_descriptor;
    word(d,0x8cu,1u);word(d,54u*2u,1u);
    word(d,8u*2u,180u);word(d,9u*2u,7u);
    word(d,19u*2u,20u);word(d,17u*2u,64u);
    word(d,15u*2u,0u);word(d,16u*2u,0u);word(d,18u*2u,128u);
    word(d,26u*2u,128u);word(d,27u*2u,127u);
    word(d,28u*2u,4u);word(d,29u*2u,127u);
    word(d,30u*2u,12u);word(d,38u*2u,52u);
    word(d,33u*2u,0x8000u);word(d,34u*2u,128u);
    word(d,35u*2u,127u);word(d,36u*2u,63u);
    word(d,37u*2u,5u);word(d,21u*2u,0x8000u);
    word(d,23u*2u,0x8000u);word(d,22u*2u,205u);
    word(d,24u*2u,205u);word(d,56u*2u,100u);
    word(d,58u*2u,0xffffu);d[0x8au]=60;
    dword(d,0x86u,0xe00000u);
    dword(d,0x76u,0x1000u);dword(d,0x7au,0x2000u);
    dword(d,0x7eu,0x1100u);dword(d,0x82u,0x1f00u);
    z.voice.descriptor=d;
    word(z.voice.descriptor,0x6au,20u);
    z.original_state.original_descriptor_verified=true;
    z.original_state.original_device_state_verified=true;
    z.original_state.original_sample_placement_verified=true;
    z.original_state.original_voice_volume_target=0u;
    z.original_state.original_pitch_target=0x8000u;
    return z;
}

int main() {
    std::array<VerifiedSf2Zone,2> zones{mono_zone(),mono_zone()};
    zones[1].key_low=65;
    zones[0].key_high=64;
    VerifiedSf2Bank bank{zones};
    const auto provider=original_verified_sf2_bank_source(bank);
    RecordedIo b{};MidiEngine e{};
    // The bank+program selector must be read from actual MIDI state.
    CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),provider)==MidiResult::MissingSource);
    CHECK(b.count==0u);
    CHECK(dispatch_midi(e,0xb0u,0u,1u,b.io(),provider)==MidiResult::Applied);
    CHECK(dispatch_midi(e,0xc0u,7u,0u,b.io(),provider)==MidiResult::Applied);
    CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),provider)==MidiResult::Applied);
    unsigned writes=0;bool saw_pitch=false,saw_sample=false,saw_trigger=false;
    for(std::size_t i=0;i<b.count;++i){
        const auto& event=b.events[i];
        if(event.type=='b'||event.type=='w'||event.type=='D')++writes;
        if(event.sel==0x00u && event.port==0xe20u && event.type=='w' && event.val==0xe000u)
            saw_pitch=true;
        if(event.sel==0xc0u && event.port==0x620u && event.type=='D' &&
           event.val==(0x40u<<24u|0x10ffu))saw_sample=true;
        if(event.sel==0xa0u && event.port==0xa20u && event.type=='w' &&
           event.val==0x053fu)saw_trigger=true;
    }
    CHECK(writes==61u && saw_pitch && saw_sample && saw_trigger);
    CHECK(e.device.voices[0].key_marker==60u);
    CHECK(dispatch_midi(e,0x90u,60u,0u,b.io(),provider)==MidiResult::Applied);
    CHECK(e.device.voices[0].key_marker==0x0ffeu);
    // Original RPN2 coarse +100 cents and RPN1 fine +10 cents modify
    // the next real compiled IP WORD, not merely channel-side metadata.
    CHECK(dispatch_midi(e,0xb0u,101u,0u,b.io(),provider)==MidiResult::Applied);
    CHECK(dispatch_midi(e,0xb0u,100u,2u,b.io(),provider)==MidiResult::Applied);
    CHECK(dispatch_midi(e,0xb0u,6u,65u,b.io(),provider)==MidiResult::Applied);
    CHECK(dispatch_midi(e,0xb0u,100u,1u,b.io(),provider)==MidiResult::Applied);
    CHECK(dispatch_midi(e,0xb0u,38u,64u,b.io(),provider)==MidiResult::Applied);
    CHECK(dispatch_midi(e,0xb0u,6u,70u,b.io(),provider)==MidiResult::Applied);
    CHECK(e.channel[0].original_coarse_tune_cents==100 &&
          e.channel[0].original_fine_tune_cents==10);
    const auto pre_tune=b.count;
    CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),provider)==MidiResult::Applied);
    bool tuned_ip=false;
    const auto expect_ip=static_cast<std::uint16_t>((0xe00000u+110u*874u)>>8u);
    for(std::size_t j=pre_tune;j<b.count;++j){
        const auto& ev=b.events[j];
        if(ev.type=='w' && ev.port==0xe20u &&
           (ev.sel&0xe0u)==0u && ev.val==expect_ip)tuned_ip=true;
    }
    CHECK(tuned_ip);
    CHECK(dispatch_midi(e,0x90u,60u,0u,b.io(),provider)==MidiResult::Applied);
    // Source matching beyond the source key range must not reuse nearest key.
    CHECK(dispatch_midi(e,0x90u,80u,100u,b.io(),provider)==MidiResult::MissingSource);
    // Reject a seventh overlapping layer, a stale/unverified source, or a
    // source-slot that no longer maps to the exact encoded descriptor.
    zones[0].original_zone_verified=false;
    CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),provider)==MidiResult::MissingSource);
    zones[0].original_zone_verified=true;
    // Stereo worklist may pick adjacent voice29/30 from authentic mode1.
    zones[0]=mono_zone();zones[1]=mono_zone();
    word(zones[0].voice.descriptor,0x8cu,2u);
    word(zones[0].voice.descriptor,0x6au,20u);
    word(zones[0].voice.descriptor,0x8eu,21u);
    word(zones[1].voice.descriptor,0x8cu,4u);
    word(zones[1].voice.descriptor,0x6au,21u);
    word(zones[1].voice.descriptor,0x8eu,20u);
    e.device.voices[31].key_marker=0x2000u;
    RecordedIo stereo_bus{};
    CHECK(dispatch_midi(e,0x90u,60u,100u,stereo_bus.io(),provider)==MidiResult::Applied);
    CHECK(e.device.voices[29].key_marker==60u && e.device.voices[30].key_marker==60u);
    unsigned triggers=0;
    std::array<std::uint8_t,2> sequence{};
    for(std::size_t i=0;i<stereo_bus.count;++i) {
        const auto& ev=stereo_bus.events[i];
        if(ev.sel>=0xa0u && ev.port==0xa20u && ev.type=='w' && ev.val==0x053fu) {
            if(triggers<2u)sequence[triggers]=static_cast<std::uint8_t>(ev.sel&31u);
            ++triggers;
        }
    }
    CHECK(triggers==2u && sequence[0]==29u && sequence[1]==30u);
    // A malformed second bank layer must be rejected BEFORE the first
    // allocator changes chip registers or native voice state. The previous
    // resolver accepted only the flags, then compiled after live allocation.
    {
        std::array<VerifiedSf2Zone,2> invalid_layers{mono_zone(),mono_zone()};
        invalid_layers[0].key_low=60;invalid_layers[0].key_high=60;
        invalid_layers[1].key_low=60;invalid_layers[1].key_high=60;
        dword(invalid_layers[1].voice.note_start_descriptor,0x76u,3u);
        VerifiedSf2Bank invalid_bank{invalid_layers};
        auto source=original_verified_sf2_bank_source(invalid_bank);
        MidiEngine isolated{};
        RecordedIo trace{};
        CHECK(dispatch_midi(isolated,0xb0u,0u,1u,trace.io(),source)==MidiResult::Applied);
        CHECK(dispatch_midi(isolated,0xc0u,7u,0u,trace.io(),source)==MidiResult::Applied);
        CHECK(dispatch_midi(isolated,0x90u,60u,100u,trace.io(),source)==MidiResult::MissingSource);
        CHECK(trace.count==0u);
        CHECK(!isolated.fault);
        CHECK(isolated.device.voices[0].key_marker==0x0ffeu);
        CHECK(isolated.device.voices[3].key_marker==0x0ffeu);
    }
    // The *verified* original melodic Program Change lookup searches bank128
    // before bank0 when requested bank is zero; channel 10 does NOT use that
    // melodic fallback. Each chosen bank stays selected even when it has no
    // zone for the particular key (no key-dependent bank fallback).
    {
        std::array<VerifiedSf2Zone,3> directory{mono_zone(),mono_zone(),mono_zone()};
        directory[0].bank=128u;directory[0].program=7u;
        directory[1].bank=0u;directory[1].program=7u;
        directory[2].bank=12u;directory[2].program=7u;
        VerifiedSf2Bank verified_dir{directory};
        verified_dir.original_melodic_program_fallback_verified=true;
        auto source=original_verified_sf2_bank_source(verified_dir);
        MidiChannel ch{};ch.program=7u;ch.bank_msb=0u;
        std::array<SourceVoice,max_source_note_layers> out{};
        std::uint8_t count=0u;
        // No implicit note-triggered program search: a source must be
        // selected by a successful Program Change first.
        CHECK(!source.resolve(source.opaque,ch,60u,100u,out,count));
        auto select=[&](std::uint8_t request) {
            ch.bank_msb=request;
            OriginalProgramSelection choice{};
            CHECK(source.select_program(source.opaque,ch,7u,choice));
            CHECK(choice.status==ProgramSelectionStatus::Found);
            ch.original_selected_bank=choice.resolved_bank;
            ch.original_selected_program=static_cast<std::uint8_t>(
                choice.resolved_program);
            ch.original_program_selection_valid=true;
        };
        select(0u);
        CHECK(source.resolve(source.opaque,ch,60u,100u,out,count));
        CHECK(count==1u && out[0].source_slot==0u); // bank128 first
        // CC0 alone must NOT silently replace the Program Change selection.
        ch.bank_msb=12u;
        CHECK(source.resolve(source.opaque,ch,60u,100u,out,count));
        CHECK(count==1u && out[0].source_slot==0u); // still bank128
        select(12u);
        CHECK(source.resolve(source.opaque,ch,60u,100u,out,count));
        CHECK(count==1u && out[0].source_slot==2u); // bank12 after C0
        select(93u);
        CHECK(source.resolve(source.opaque,ch,60u,100u,out,count));
        CHECK(count==1u && out[0].source_slot==0u); // fallback bank128
        // Drum-channel marker bypasses the MELODIC source lookup.
        ch.bank_msb=0u;ch.original_channel_is_percussion=true;
        CHECK(source.resolve(source.opaque,ch,60u,100u,out,count));
        CHECK(count==1u && out[0].source_slot==1u); // direct drum bank0
        ch.original_channel_is_percussion=false;
        directory[0].key_low=65u;directory[0].key_high=67u;
        CHECK(!source.resolve(source.opaque,ch,60u,100u,out,count));
        // Selected bank128 has no C4 zone: no key-dependent bank fallback.
        directory[0].key_low=60u;
        directory[0].key_high=64u;
        directory[0].program=6u;
        // Removing the selected preset does not reselect until a new C0.
        CHECK(!source.resolve(source.opaque,ch,60u,100u,out,count));
        select(0u); // now bank128 program7 unavailable; select bank0
        CHECK(source.resolve(source.opaque,ch,60u,100u,out,count));
        CHECK(count==1u && out[0].source_slot==1u);
    }
    // Two independently loaded banks can expose IDENTICAL bank/program
    // bytes. Original +0x6D05 selects one exact device slot and row.
    // A naive note-source scan by bank/program alone would start BOTH and
    // silently layer unrelated SoundFonts. Reject it at source identity.
    {
        std::array<VerifiedSf2Zone,3> duplicates{mono_zone(),mono_zone(),mono_zone()};
        for(auto& z:duplicates){
            z.bank=128u;z.program=7u;
            z.original_preset_identity_verified=true;
        }
        duplicates[0].original_bank_slot=130u;
        duplicates[0].original_preset_row=0u;
        duplicates[1].original_bank_slot=131u;
        duplicates[1].original_preset_row=0u;
        duplicates[2].original_bank_slot=130u;
        duplicates[2].original_preset_row=1u;
        std::array<OriginalProgramBankSlot,original_program_bank_slots> slots{};
        const std::array<OriginalPresetTableRow,2> row130{{
            {1u,7u,128u},{0xffffu,0u,0u}}};
        const std::array<OriginalPresetTableRow,2> row131{{
            {1u,7u,128u},{0xffffu,0u,0u}}};
        slots[130].rows=row130;slots[131].rows=row131;
        VerifiedSf2Bank directory{duplicates};
        directory.original_program_map={true,slots};
        auto source=original_verified_sf2_bank_source(directory);
        MidiEngine engine{};RecordedIo trace{};
        CHECK(dispatch_midi(engine,0xc0u,7u,0u,trace.io(),source)==MidiResult::Applied);
        CHECK(engine.channel[0].original_selected_bank_slot==130u);
        CHECK(engine.channel[0].original_selected_preset_row==0u);
        CHECK(dispatch_midi(engine,0x90u,60u,100u,trace.io(),source)==MidiResult::Applied);
        unsigned triggers=0u;
        for(std::size_t i=0;i<trace.count;++i) {
            const auto& ev=trace.events[i];
            if((ev.sel&0xe0u)==0xa0u && ev.port==0xa20u &&
               ev.type=='w' && ev.val==0x053fu)++triggers;
        }
        CHECK(triggers==1u); // not three same-bank/program voices
        CHECK(engine.device.voices[0].key_marker==60u);
        CHECK(engine.device.voices[3].key_marker==0x0ffeu);
        // An unverified mapping must not pass merely because its bytes
        // happen to equal the chosen bank/program.
        duplicates[0].original_preset_identity_verified=false;
        CHECK(dispatch_midi(engine,0x90u,61u,100u,trace.io(),source)==MidiResult::MissingSource);
    }
    // Direct 288-slot original VXD program-directory integration (not
    // merely the earlier bank/program existence heuristic). Synthetic
    // resource records are shaped like authenticated 26-byte source rows.
    {
        std::array<VerifiedSf2Zone,1> mapped_zones{mono_zone()};
        mapped_zones[0].bank=128u;
        mapped_zones[0].original_preset_identity_verified=true;
        mapped_zones[0].original_bank_slot=130u;
        mapped_zones[0].original_preset_row=0u;
        std::array<OriginalProgramBankSlot,original_program_bank_slots> table{};
        const std::array<OriginalPresetTableRow,2> rows{{
            {1u,7u,128u},{0xffffu,0u,0u}}};
        table[130].rows=rows;
        VerifiedSf2Bank mapped{mapped_zones};
        mapped.original_program_map={true,table};
        const auto origin=original_verified_sf2_bank_source(mapped);
        MidiEngine source_engine{};RecordedIo source_bus{};
        CHECK(dispatch_midi(source_engine,0xb0u,0u,2u,source_bus.io(),origin)
              ==MidiResult::Applied);
        CHECK(dispatch_midi(source_engine,0xc0u,7u,0u,source_bus.io(),origin)
              ==MidiResult::Applied);
        // Requested bank2 falls back to AUTHENTIC bank128 entry located
        // in slot130, exactly as source +0x6E71 scans.
        CHECK(dispatch_midi(source_engine,0x90u,60u,100u,source_bus.io(),origin)
              ==MidiResult::Applied);
        unsigned outs=0u;
        for(std::size_t i=0;i<source_bus.count;++i) {
            if(source_bus.events[i].type!='r' &&
               source_bus.events[i].type!='d')++outs;
        }
        CHECK(outs==61u);
        CHECK(source_engine.device.voices[0].key_marker==60u);
    }
    // Original channel10 percussion path: +0x6D05 dispatches to +0x6D55,
    // maps requested GM program0 to actual source program128, using the
    // source instrument BANK=128. It is NOT melodic bank fallback.
    {
        std::array<VerifiedSf2Zone,1> drums{mono_zone()};
        drums[0].bank=128u;drums[0].program=128u;
        drums[0].original_preset_identity_verified=true;
        drums[0].original_bank_slot=128u;
        drums[0].original_preset_row=0u;
        std::array<OriginalProgramBankSlot,original_program_bank_slots> slots{};
        const std::array<OriginalPresetTableRow,2> rows{{
            {1u,128u,128u},{0xffffu,0u,0u}}};
        slots[128].rows=rows;
        VerifiedSf2Bank bank_drums{drums};
        bank_drums.original_program_map={true,slots};
        auto sound=original_verified_sf2_bank_source(bank_drums);
        MidiEngine e{};RecordedIo bus{};
        CHECK(dispatch_midi(e,0xc9u,0u,0u,bus.io(),sound)==MidiResult::Applied);
        CHECK(dispatch_midi(e,0x99u,60u,127u,bus.io(),sound)==MidiResult::Applied);
        CHECK(e.device.voices[0].key_marker==0x093cu);
        CHECK(dispatch_midi(e,0x89u,60u,0u,bus.io(),sound)==MidiResult::Applied);
        CHECK(e.device.voices[0].key_marker==0x0ffeu);
    }
    // The original Win95 LCOD+0x6D05 resolves program+bank at MIDI C0.
    // A later CC0 updates the *requested* bank but must not retroactively
    // change the physical selected preset until another Program Change.
    // Two distinguishable, authenticated-shaped zone descriptors prove
    // this at the actual EMU8000 PSST write, not only via channel fields.
    {
        std::array<VerifiedSf2Zone,2> latch_zones{mono_zone(),mono_zone()};
        latch_zones[0].bank=128u; latch_zones[0].program=7u;
        latch_zones[0].original_preset_identity_verified=true;
        latch_zones[0].original_bank_slot=130u;
        latch_zones[0].original_preset_row=0u;
        latch_zones[1].bank=2u;   latch_zones[1].program=7u;
        latch_zones[1].original_preset_identity_verified=true;
        latch_zones[1].original_bank_slot=2u;
        latch_zones[1].original_preset_row=0u;
        auto& chosen=latch_zones[1].voice;
        dword(chosen.note_start_descriptor,0x76u,0x3000u);
        dword(chosen.note_start_descriptor,0x7au,0x4000u);
        dword(chosen.note_start_descriptor,0x7eu,0x3100u);
        dword(chosen.note_start_descriptor,0x82u,0x3f00u);
        chosen.descriptor=chosen.note_start_descriptor;
        std::array<OriginalProgramBankSlot,original_program_bank_slots> table{};
        const std::array<OriginalPresetTableRow,2> bank2_rows{{
            {1u,7u,2u},{0xffffu,0u,0u}}};
        const std::array<OriginalPresetTableRow,2> bank128_rows{{
            {1u,7u,128u},{0xffffu,0u,0u}}};
        table[2].rows=bank2_rows;
        table[130].rows=bank128_rows;
        VerifiedSf2Bank latch_bank{latch_zones};
        latch_bank.original_program_map={true,table};
        auto src=original_verified_sf2_bank_source(latch_bank);
        MidiEngine engine{}; RecordedIo trace{};
        // Requested 3 is absent => the original bank-128 fallback at C0.
        CHECK(dispatch_midi(engine,0xb0u,0u,3u,trace.io(),src)==MidiResult::Applied);
        CHECK(dispatch_midi(engine,0xc0u,7u,0u,trace.io(),src)==MidiResult::Applied);
        CHECK(engine.channel[0].original_program_selection_valid);
        CHECK(engine.channel[0].original_selected_bank==128u);
        CHECK(engine.channel[0].original_selected_bank_slot==130u);
        // CC0 changes only requested bank. NoteOn must use latched 128.
        CHECK(dispatch_midi(engine,0xb0u,0u,2u,trace.io(),src)==MidiResult::Applied);
        CHECK(engine.channel[0].bank_msb==2u);
        CHECK(engine.channel[0].original_selected_bank==128u);
        auto count_writes=[](RecordedIo const& bus,std::size_t start,
                              std::uint32_t low24) noexcept {
            unsigned count=0u;
            for(auto i=start;i<bus.count;++i) {
                const auto& event=bus.events[i];
                if(event.port==0x620u && event.type=='D' &&
                   (event.sel&0xe0u)==0xc0u &&
                   (event.val&0xffffffu)==low24)++count;
            }
            return count;
        };
        auto mark=trace.count;
        CHECK(dispatch_midi(engine,0x90u,60u,100u,trace.io(),src)==MidiResult::Applied);
        CHECK(count_writes(trace,mark,0x10ffu)==1u);
        CHECK(count_writes(trace,mark,0x30ffu)==0u);
        CHECK(dispatch_midi(engine,0x80u,60u,0u,trace.io(),src)==MidiResult::Applied);
        // Repeating Program Change latches requested bank 2 (exact source).
        mark=trace.count;
        CHECK(dispatch_midi(engine,0xc0u,7u,0u,trace.io(),src)==MidiResult::Applied);
        CHECK(trace.count==mark); // C0 is state-only in this verified case
        CHECK(engine.channel[0].original_selected_bank==2u);
        CHECK(engine.channel[0].original_selected_bank_slot==2u);
        mark=trace.count;
        CHECK(dispatch_midi(engine,0x90u,60u,100u,trace.io(),src)==MidiResult::Applied);
        CHECK(count_writes(trace,mark,0x30ffu)==1u);
        CHECK(count_writes(trace,mark,0x10ffu)==0u);
        // Real source LCOD+0x6D26 first clears all prior selected-preset
        // identifiers. Missing C0 must INVALIDATE the old program; it
        // must not keep making sound from a stale, previously valid bank.
        mark=trace.count;
        CHECK(dispatch_midi(engine,0xc0u,9u,0u,trace.io(),src)==MidiResult::MissingSource);
        CHECK(trace.count==mark && !engine.fault);
        CHECK(engine.channel[0].program==9u);
        CHECK(!engine.channel[0].original_program_selection_valid);
        CHECK(engine.channel[0].original_selected_bank_slot==0u);
        CHECK(engine.channel[0].original_selected_preset_row==0u);
        CHECK(engine.channel[0].original_selected_program==0u);
        mark=trace.count;
        CHECK(dispatch_midi(engine,0x90u,60u,100u,trace.io(),src)==MidiResult::MissingSource);
        CHECK(trace.count==mark);
        // Recover by selecting the previously verified program again.
        CHECK(dispatch_midi(engine,0xc0u,7u,0u,trace.io(),src)==MidiResult::Applied);
        CHECK(engine.channel[0].original_program_selection_valid);
        CHECK(engine.channel[0].original_selected_bank==2u);
    }
    std::puts("P003_VERIFIED_SHAPED_SF2_E2E_BANK_PROGRAM_STEREO_PASS");
}
