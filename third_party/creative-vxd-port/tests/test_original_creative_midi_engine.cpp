#include "OriginalCreativeMidiEngine.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace awe64::original_vxd;
#define CHECK(v) do { if(!(v)){std::fprintf(stderr,"MIDI_TEST_FAIL line %d: %s\n",__LINE__,#v);std::exit(1);} } while(false)

struct Event { char kind;std::uint16_t port;std::uint32_t value;std::uint8_t selector;};
struct Bus {
    std::array<std::uint32_t,32> vtft{},ccca{},psst{},csl{};
    std::array<std::uint16_t,32> sus{};
    std::uint8_t selector=0;
    std::vector<Event> trace;
    std::size_t fail_after=static_cast<std::size_t>(-1);
    static bool out8(void* p,std::uint16_t port,std::uint8_t v) noexcept {
        auto& b=*static_cast<Bus*>(p);
        if(port!=0xe22u || b.trace.size()>=b.fail_after)return false;
        b.selector=v;b.trace.push_back({'b',port,v,v});return true;
    }
    static bool out16(void* p,std::uint16_t port,std::uint16_t v) noexcept {
        auto& b=*static_cast<Bus*>(p);
        if(port!=0xa20u && port!=0xa22u && port!=0xe20u)return false;
        if(b.trace.size()>=b.fail_after)return false;
        const unsigned idx=b.selector&31u;
        if(port==0xa20u && (b.selector&0xe0u)==0xa0u)b.sus[idx]=v;
        b.trace.push_back({'w',port,v,b.selector});return true;
    }
    static bool out32(void* p,std::uint16_t port,std::uint32_t v) noexcept {
        auto& b=*static_cast<Bus*>(p);
        if(port!=0x620u && port!=0xa20u)return false;
        if(b.trace.size()>=b.fail_after)return false;
        const unsigned idx=b.selector&31u;
        const auto reg=b.selector&0xe0u;
        if(port==0x620u && reg==0x60u)b.vtft[idx]=v;
        if(port==0x620u && reg==0xc0u)b.psst[idx]=v;
        if(port==0x620u && reg==0xe0u)b.csl[idx]=v;
        if(port==0xa20u && reg==0x00u)b.ccca[idx]=v;
        b.trace.push_back({'D',port,v,b.selector});return true;
    }
    static bool in16(void* p,std::uint16_t port,std::uint16_t& v) noexcept {
        auto& b=*static_cast<Bus*>(p);
        if(port!=0xa20u || (b.selector&0xe0u)!=0xa0u)return false;
        v=b.sus[b.selector&31u];b.trace.push_back({'r',port,v,b.selector});return true;
    }
    static bool in32(void* p,std::uint16_t port,std::uint32_t& v) noexcept {
        auto& b=*static_cast<Bus*>(p);
        const auto idx=b.selector&31u;
        const auto reg=b.selector&0xe0u;
        if(port==0x620u && reg==0x60u)v=b.vtft[idx];
        else if(port==0xa20u && reg==0x00u)v=b.ccca[idx];
        else if(port==0x620u && reg==0xc0u)v=b.psst[idx];
        else if(port==0x620u && reg==0xe0u)v=b.csl[idx];
        else return false;
        b.trace.push_back({'d',port,v,b.selector});return true;
    }
    [[nodiscard]] PortIo io() noexcept {return {this,out8,out16,in16,in32,out32};}
};

void set_word(Descriptor& d,std::size_t offset,std::uint16_t v) {
    d[offset]=static_cast<std::uint8_t>(v);
    d[offset+1]=static_cast<std::uint8_t>(v>>8u);
}
struct Provider {
    unsigned requested=0,compiled=0;
    std::uint8_t layers=1;
    bool refuse_compile=false, refuse_resolve=false, overflow=false;
    bool stereo=true;
    bool verified_note_start=false;
    std::uint16_t note_mode=0u;
    std::uint32_t note_start=0x1000u;
    std::uint32_t note_end=0x2000u;
    std::uint32_t forced_release_end=0u;
    MidiChannel last_channel{};
    static bool resolve(void* opaque,MidiChannel const& ch,std::uint8_t key,
        std::uint8_t velocity,std::array<SourceVoice,max_source_note_layers>& output,
        std::uint8_t& count) noexcept {
        auto& p=*static_cast<Provider*>(opaque);
        (void)key;(void)velocity;
        p.last_channel=ch;
        ++p.requested;
        if(p.refuse_resolve)return false;
        count=p.layers;
        if(count>max_source_note_layers)return true;
        for(unsigned i=0;i<count;++i) {
            output[i].source_verified=true;
            output[i].release_end=p.forced_release_end;
            if(p.verified_note_start){
                output[i].note_start_descriptor_verified=true;
                set_word(output[i].note_start_descriptor,0x6cu,p.note_mode);
                set_word(output[i].note_start_descriptor,0x8cu,1u);
                for(unsigned off=0;off<4u;++off){
                    output[i].note_start_descriptor[0x76u+off]=
                        static_cast<std::uint8_t>(p.note_start>>(off*8u));
                    output[i].note_start_descriptor[0x7au+off]=
                        static_cast<std::uint8_t>(p.note_end>>(off*8u));
                }
            }
            output[i].original_pitch_base=0x1000u;
            output[i].original_pitch_base_verified=true;
            output[i].descriptor[0x3c]=0x12u;
            output[i].descriptor[0x4c]=0x34u;
            set_word(output[i].descriptor,0x6au,static_cast<std::uint16_t>(20u+i));
            if(p.layers==2u && p.stereo) {
                if(i==0u) {
                    set_word(output[i].descriptor,0x8cu,2u);
                    set_word(output[i].descriptor,0x8eu,21u);
                }else {
                    set_word(output[i].descriptor,0x8cu,4u);
                    set_word(output[i].descriptor,0x8eu,20u);
                }
            } else {
                set_word(output[i].descriptor,0x8cu,1u);
            }
        }
        return true;
    }
    static bool compile(void* opaque,SourceVoice const& voice,
                        std::uint8_t physical,std::uint8_t key,
                        OriginalVoiceProgram& plan) noexcept {
        auto& p=*static_cast<Provider*>(opaque);
        (void)voice;(void)key;
        ++p.compiled;
        if(p.refuse_compile)return false;
        plan.source_verified=true;
        for(unsigned i=0;i<plan.data.size();++i)plan.data[i]=0x40u+i;
        plan.data[0]=0x0000ffffu;
        plan.data[1]=0x0000ffffu;
        plan.data[23]=0u; // final VTFT; keep hardware idle if inactive
        plan.data[27]=0x047fu; // source ending DCYSUSV
        if(p.overflow)plan.data[27]=0x10000u;
        (void)physical;
        return true;
    }
    [[nodiscard]] SoundFontSource provider() noexcept {
        return {this,resolve,compile};
    }
};

[[nodiscard]] MidiEngine fresh() {
    MidiEngine e{};
    for(auto& v:e.device.voices) {
        v.key_marker=0x0ffeu;
        v.sample_end=0u;
    }
    return e;
}

int main() {
    // 61 typed OUT writes in one source NoteOn: 5 allocation prologue
    // (VTFT select, 2x release, 2x reset) plus 56 main writes.
    {
        MidiEngine e=fresh();Bus b;Provider src{};
        const auto r=dispatch_midi(e,0x90u,60u,100u,b.io(),src.provider());
        CHECK(r==MidiResult::Applied && src.requested==1u && src.compiled==1u);
        CHECK(e.device.voices[0].key_marker==0x003cu);
        CHECK(e.device.voices[0].volume_release==0x34u);
        CHECK(e.device.voices[0].modulation_release==0x12u);
        unsigned physical_outs=0;
        for(const auto& event:b.trace)if(event.kind=='b'||event.kind=='w'||event.kind=='D')++physical_outs;
        CHECK(physical_outs==61u);
        CHECK(b.trace[0].kind=='b' && b.trace[0].value==0x60u);
        // Last DATA1 operation is the note-on trigger for voice 0.
        CHECK(b.trace.back().kind=='w' && b.trace.back().selector==0xa0u &&
              b.trace.back().value==0x047fu);
        // Velocity-zero original NoteOn takes NoteOff path; no new resolve.
        CHECK(dispatch_midi(e,0x90u,60u,0u,b.io(),src.provider())==MidiResult::Applied);
        CHECK(src.requested==1u);
        CHECK(e.device.voices[0].key_marker==0x0ffeu);
    }
    // MIDI serial running status, realtime interleaved, bank and program.
    {
        MidiEngine e=fresh();Bus b;Provider src{};
        CHECK(feed_midi_byte(e,0x90u,b.io(),src.provider())==MidiResult::Incomplete);
        CHECK(feed_midi_byte(e,60u,b.io(),src.provider())==MidiResult::Incomplete);
        CHECK(feed_midi_byte(e,0xf8u,b.io(),src.provider())==MidiResult::IgnoredRealtime);
        CHECK(feed_midi_byte(e,90u,b.io(),src.provider())==MidiResult::Applied);
        CHECK(feed_midi_byte(e,62u,b.io(),src.provider())==MidiResult::Incomplete);
        CHECK(feed_midi_byte(e,96u,b.io(),src.provider())==MidiResult::Applied);
        CHECK(e.device.voices[0].key_marker==60u);
        CHECK(e.device.voices[3].key_marker==62u);
        CHECK(feed_midi_byte(e,0xc0u,b.io(),src.provider())==MidiResult::Incomplete);
        CHECK(feed_midi_byte(e,17u,b.io(),src.provider())==MidiResult::Applied);
        CHECK(e.channel[0].program==17u);
        CHECK(dispatch_midi(e,0xb0u,0u,3u,b.io(),src.provider())==MidiResult::Applied);
        CHECK(dispatch_midi(e,0xb0u,32u,4u,b.io(),src.provider())==MidiResult::Unsupported);
        CHECK(e.channel[0].bank_msb==3u && e.channel[0].bank_lsb==0u);
        CHECK(dispatch_midi(e,0xb0u,91u,100u,b.io(),src.provider())==MidiResult::Applied);
        CHECK(e.channel[0].reverb==90u);
        CHECK(dispatch_midi(e,0xb0u,93u,100u,b.io(),src.provider())==MidiResult::Applied);
        CHECK(e.channel[0].chorus==90u);
        CHECK(dispatch_midi(e,0xb0u,7u,99u,b.io(),src.provider())==MidiResult::Unsupported);
        CHECK(e.channel[0].volume==99u);
        CHECK(dispatch_midi(e,0xb0u,10u,35u,b.io(),src.provider())==MidiResult::Unsupported);
        e.channel[0].original_pitch_tune=16;
        const auto before_pitch=b.trace.size();
        CHECK(dispatch_midi(e,0xe0u,0u,96u,b.io(),src.provider())==MidiResult::Applied);
        CHECK(e.channel[0].pitch_bend==12288u);
        CHECK(e.channel[0].original_bend_delta==341);
        CHECK(b.trace.size()==before_pitch+4u);
        CHECK(b.trace[before_pitch].port==0xe22u && b.trace[before_pitch].value==0u);
        CHECK(b.trace[before_pitch+1u].port==0xe20u && b.trace[before_pitch+1u].value==0x1165u);
        CHECK(b.trace[before_pitch+2u].port==0xe22u && b.trace[before_pitch+2u].value==3u);
        CHECK(b.trace[before_pitch+3u].port==0xe20u && b.trace[before_pitch+3u].value==0x1165u);
        CHECK(feed_midi_byte(e,0xf0u,b.io(),src.provider())==MidiResult::Incomplete);
        CHECK(feed_midi_byte(e,33u,b.io(),src.provider())==MidiResult::Incomplete);
        CHECK(feed_midi_byte(e,0xf7u,b.io(),src.provider())==MidiResult::Incomplete);
        CHECK(feed_midi_byte(e,60u,b.io(),src.provider())==MidiResult::Invalid);
        CHECK(feed_midi_byte(e,0xf2u,b.io(),src.provider())==MidiResult::Incomplete);
        CHECK(feed_midi_byte(e,0xf8u,b.io(),src.provider())==MidiResult::IgnoredRealtime);
        CHECK(feed_midi_byte(e,1u,b.io(),src.provider())==MidiResult::Incomplete);
        CHECK(feed_midi_byte(e,2u,b.io(),src.provider())==MidiResult::Incomplete);
        CHECK(feed_midi_byte(e,3u,b.io(),src.provider())==MidiResult::Invalid);
    }
    // Sustain CC64 holds note and releases on pedal lift.
    {
        MidiEngine e=fresh();Bus b;Provider src{};
        CHECK(dispatch_midi(e,0x92u,61u,80u,b.io(),src.provider())==MidiResult::Applied);
        CHECK(dispatch_midi(e,0xb2u,64u,127u,b.io(),src.provider())==MidiResult::Applied);
        const std::size_t before=b.trace.size();
        CHECK(dispatch_midi(e,0x82u,61u,64u,b.io(),src.provider())==MidiResult::Applied);
        CHECK(b.trace.size()==before && e.device.voices[0].key_marker==0x02ffu);
        CHECK(dispatch_midi(e,0xb2u,64u,0u,b.io(),src.provider())==MidiResult::Applied);
        CHECK(b.trace.size()==before+4u);
        CHECK(e.device.voices[0].key_marker==0x0ffeu);
    }
    // Two original mono layers: all allocation prologues FIRST, then
    // one COMPLETE 28-pair main+trigger per layer (source seq726191...).
    {
        MidiEngine e=fresh();Bus b;Provider src{};
        src.layers=2u;src.stereo=false;
        CHECK(dispatch_midi(e,0x90u,65u,127u,b.io(),src.provider())==MidiResult::Applied);
        CHECK(e.device.voices[0].key_marker==65u);
        CHECK(e.device.voices[3].key_marker==65u);
        std::vector<unsigned> triggers;
        for(const auto& ev:b.trace) {
            if(ev.kind=='w' && ev.port==0xa20u && ev.value==0x047fu)
                triggers.push_back(ev.selector&31u);
        }
        CHECK(triggers.size()==2u && triggers[0]==0u && triggers[1]==3u);
        CHECK(b.trace[b.trace.size()-1u].selector==0xa3u);
        // The first voice's trigger is NOT deferred to the end of the
        // second mono layer. It precedes voice3's main VTFT write.
        std::size_t first_trigger=0,second_main=0;
        for(std::size_t j=0;j<b.trace.size();++j) {
            if(b.trace[j].kind=='w' && b.trace[j].port==0xa20u &&
               b.trace[j].value==0x047fu && b.trace[j].selector==0xa0u)
                first_trigger=j;
            if(b.trace[j].kind=='D' && b.trace[j].selector==0x63u &&
               b.trace[j].value==0xffffu)second_main=j;
        }
        CHECK(first_trigger>0u && second_main>first_trigger);
        // Both selected voices must have completed the allocation VTFT
        // reset BEFORE any first main register write (stack-ordered).
        unsigned reset_count=0;
        std::size_t first_main=0;
        for(std::size_t i=0;i<b.trace.size();++i) {
            if(b.trace[i].kind=='D' && b.trace[i].value==0x0000ffffu) {
                ++reset_count;
                if(reset_count==2u){first_main=i+1u;break;}
            }
        }
        CHECK(reset_count==2u && first_main<b.trace.size());
        CHECK(b.trace[first_main].kind=='b' && b.trace[first_main].value==0x60u);
    }
    // Source paired stereo, actual original pair 29+30 after voice31
    // is unavailable. Partner (left) is programmed to 30; right to 29.
    {
        MidiEngine e=fresh();Bus b;Provider src{};
        e.device.voices[31].key_marker=0x2000u;
        src.layers=2u;
        CHECK(dispatch_midi(e,0x90u,67u,100u,b.io(),src.provider())==MidiResult::Applied);
        CHECK(src.compiled==2u);
        CHECK(e.device.voices[29].key_marker==67u && e.device.voices[30].key_marker==67u);
        unsigned left=0,right=0;
        for(auto const& ev:b.trace) {
            if(ev.kind=='w' && ev.port==0xa20u && ev.value==0x047fu) {
                if(ev.selector==static_cast<std::uint8_t>(0xa0u|30u))++left;
                if(ev.selector==static_cast<std::uint8_t>(0xa0u|29u))++right;
            }
        }
        CHECK(left==1u && right==1u);
        CHECK(b.trace[b.trace.size()-1u].selector==static_cast<std::uint8_t>(0xa0u|30u));
        CHECK(b.trace[b.trace.size()-3u].selector==static_cast<std::uint8_t>(0xa0u|29u));
        // Original +0x5BF4: partner voice 30 is programmed first,
        // triggering right 29 second, while NOTE TRIGGERS reverse order.
        std::vector<unsigned> first_body_voices;
        for(std::size_t j=0;j<b.trace.size();++j) {
            if(j+1u<b.trace.size() && b.trace[j].kind=='b' &&
               (b.trace[j].value&0xe0u)==0x60u &&
               b.trace[j+1u].kind=='D' &&
               b.trace[j+1u].value==0x0000ffffu)
                first_body_voices.push_back(b.trace[j].value&31u);
        }
        CHECK(first_body_voices.size()>=4u);
        CHECK(first_body_voices[first_body_voices.size()-2u]==30u ||
              first_body_voices[first_body_voices.size()-2u]==29u);
        CHECK(dispatch_midi(e,0x90u,67u,0u,b.io(),src.provider())==MidiResult::Applied);
        CHECK(e.device.voices[29].key_marker==0x0ffeu && e.device.voices[30].key_marker==0x0ffeu);
    }
    // Strict missing-source, invalid physical voice and hardware error.
    {
        MidiEngine e=fresh();Bus b;Provider src{};
        src.refuse_resolve=true;
        CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),src.provider())==MidiResult::MissingSource);
        CHECK(b.trace.empty() && !e.fault);
        src.refuse_resolve=false;src.overflow=true;
        CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),src.provider())==MidiResult::MissingSource);
        CHECK(e.fault);
        CHECK(dispatch_midi(e,0xb0u,7u,80u,b.io(),src.provider())==MidiResult::FaultLatched);
    }
    {
        MidiEngine e=fresh();Bus b;Provider src{};
        b.fail_after=1u;
        CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),src.provider())==MidiResult::PortError);
        CHECK(e.fault);
    }
    {
        MidiEngine e=fresh();Bus b;Provider src{};
        CHECK(dispatch_midi(e,0x90u,128u,20u,b.io(),src.provider())==MidiResult::Invalid);
        CHECK(dispatch_midi(e,0x70u,64u,90u,b.io(),src.provider())==MidiResult::Invalid);
        CHECK(dispatch_midi(e,0xa0u,64u,90u,b.io(),src.provider())==MidiResult::Unsupported);
        CHECK(b.trace.empty());
    }
    // Signed x86 IDIV truncation and original unsigned saturation order:
    // a negative pitch target wraps above 0xffff, so source writes FFFF.
    {
        MidiEngine e=fresh();Bus b;Provider src{};
        e.device.voices[0].key_marker=static_cast<std::uint16_t>(0x0100u|60u);
        e.device.voices[0].original_pitch_base_verified=true;
        e.device.voices[0].original_pitch_base=0u;
        CHECK(dispatch_midi(e,0xe1u,0u,0u,b.io(),src.provider())==MidiResult::Applied);
        CHECK(e.channel[1].original_bend_delta==-682);
        CHECK(b.trace.size()==2u && b.trace[1].port==0xe20u &&
              b.trace[1].value==0xffffu);
    }
    // Unknown pitch-base source must not silently invent an active-voice IP.
    {
        MidiEngine e=fresh();Bus b;Provider src{};
        e.device.voices[2].key_marker=0x023cu;
        CHECK(dispatch_midi(e,0xe2u,0u,64u,b.io(),src.provider())==MidiResult::MissingSource);
        CHECK(b.trace.empty());
    }
    // Source x86 LCOD+0x5B87..+0x5BE1 independently distinguishes the
    // allocator cached end from the NoteOff retained-release end. The
    // pre-port engine dropped sample_end entirely, hiding later CCCA reads.
    {
        MidiEngine e=fresh();Bus b;Provider p{};
        p.verified_note_start=true;p.note_mode=0u;p.note_end=0x2000u;
        CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),p.provider())==MidiResult::Applied);
        CHECK(e.device.voices[0].sample_end==0x2004u);
        CHECK(e.device.voices[0].release_sample_end==0u);
        CHECK(e.device.voices[0].blocked && e.device.mode_count==33u);
        const auto before=b.trace.size();
        CHECK(dispatch_midi(e,0x90u,62u,100u,b.io(),p.provider())==MidiResult::Applied);
        bool read_real_ccca=false;
        for(std::size_t i=before;i<b.trace.size();++i){
            const auto& ev=b.trace[i];
            if(ev.kind=='d' && ev.port==0xa20u && ev.selector==0u)
                read_real_ccca=true;
        }
        CHECK(read_real_ccca); // allocator actually observes cached end
    }
    {
        MidiEngine e=fresh();Bus b;Provider p{};
        p.verified_note_start=true;p.note_mode=1u;p.note_end=0x2300u;
        CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),p.provider())==MidiResult::Applied);
        CHECK(e.device.voices[0].sample_end==0u);
        CHECK(e.device.voices[0].release_sample_end==0u);
    }
    {
        MidiEngine e=fresh();Bus b;Provider p{};
        p.verified_note_start=true;p.note_mode=3u;p.note_end=0x2400u;
        CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),p.provider())==MidiResult::Applied);
        CHECK(e.device.voices[0].sample_end==0u);
        CHECK(e.device.voices[0].release_sample_end==0x2404u);
        CHECK(dispatch_midi(e,0x80u,60u,0u,b.io(),p.provider())==MidiResult::Applied);
        CHECK(e.device.voices[0].key_marker==0x0ffeu);
        CHECK(e.device.voices[0].sample_end==0x2404u);
    }
    {
        MidiEngine e=fresh();Bus b;Provider p{};
        p.verified_note_start=true;p.note_start=0x200100u;
        p.note_end=0x210000u;p.note_mode=1u;
        e.device.voices[0].blocked=true;
        CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),p.provider())==MidiResult::Applied);
        CHECK(e.device.voices[0].sample_end==0u);
        CHECK(!e.device.voices[0].blocked && e.device.mode_count==31u);
    }
    {
        MidiEngine e=fresh();Bus b;Provider p{};
        p.verified_note_start=true;p.note_start=0x1000u;
        p.note_end=0x2000u;p.note_mode=1u;
        e.device.voices[0].blocked=true;
        CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),p.provider())==MidiResult::Applied);
        CHECK(e.device.voices[0].blocked && e.device.mode_count==32u);
    }
    {   // Source bank with internally inconsistent geometry: no chip I/O.
        MidiEngine e=fresh();Bus b;Provider p{};
        p.verified_note_start=true;p.note_start=0x5000u;p.note_end=0x2000u;
        CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),p.provider())==MidiResult::MissingSource);
        CHECK(b.trace.empty() && e.device.mode_count==32u);
    }
    for(const std::uint16_t mode:{4u,0xffffu}) {
        MidiEngine e=fresh();Bus b;Provider p{};
        p.verified_note_start=true;p.note_mode=mode;
        CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),p.provider())==MidiResult::MissingSource);
        CHECK(b.trace.empty() && e.device.voices[0].key_marker==0x0ffeu);
    }
    {
        MidiEngine e=fresh();Bus b;Provider p{};
        p.verified_note_start=true;p.note_mode=0u;p.note_end=0x00fffffcu;
        CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),p.provider())==MidiResult::MissingSource);
        CHECK(b.trace.empty());
    }
    {
        MidiEngine e=fresh();Bus b;Provider p{};
        p.verified_note_start=true;p.note_mode=3u;p.note_end=0x2000u;
        p.forced_release_end=0x2104u; // contradictory source claim
        CHECK(dispatch_midi(e,0x90u,60u,100u,b.io(),p.provider())==MidiResult::MissingSource);
        CHECK(b.trace.empty());
    }
    std::puts("P003_PORTABLE_ORIGINAL_MIDI_STREAM_AND_NOTEON_ENGINE_TEST_PASS");
}
