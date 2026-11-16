#include "OriginalCreativeVxdLive.hpp"
#include "OriginalCreativeStereoWorklist.hpp"
#include "OriginalCreativeChipBridge.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace awe64::original_vxd;
#define REQUIRE(expr) do {if(!(expr)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#expr);std::exit(1);}} while(false)

struct Event {char kind;std::uint16_t port;std::uint32_t value;};
struct Bus {
    std::array<std::uint32_t,32> vtft{}, ccca{}, csl{}, psst{};
    std::array<std::uint16_t,32> sus{};
    std::uint8_t selector=0;
    bool fail_next_read=false;
    std::vector<Event> events;
    static bool out8(void* ptr,std::uint16_t port,std::uint8_t val) noexcept {
        auto& b=*static_cast<Bus*>(ptr);
        if(port!=0x0e22u)return false;
        b.selector=val;b.events.push_back({'b',port,val});return true;
    }
    static bool out16(void* ptr,std::uint16_t port,std::uint16_t val) noexcept {
        auto& b=*static_cast<Bus*>(ptr);
        if(port!=0x0a20u ||
           ((b.selector&0xe0u)!=0xa0u && (b.selector&0xe0u)!=0xe0u) ||
           ((val&0x8000u)==0u && val!=0xffu))return false;
        b.events.push_back({'w',port,val});return true;
    }
    static bool in16(void* ptr,std::uint16_t port,std::uint16_t& val) noexcept {
        auto& b=*static_cast<Bus*>(ptr);
        if(port!=0x0a20u || (b.selector&0xe0u)!=0xa0u || b.fail_next_read)return false;
        val=b.sus[b.selector&31u];b.events.push_back({'r',port,val});return true;
    }
    static bool in32(void* ptr,std::uint16_t port,std::uint32_t& val) noexcept {
        auto& b=*static_cast<Bus*>(ptr);
        if(b.fail_next_read)return false;
        const auto voice=b.selector&31u;
        if(port==0x0620u && (b.selector&0xe0u)==0x60u)val=b.vtft[voice];
        else if(port==0x0a20u && (b.selector&0xe0u)==0u)val=b.ccca[voice];
        else if(port==0x0620u && (b.selector&0xe0u)==0xe0u)val=b.csl[voice];
        else if(port==0x0620u && (b.selector&0xe0u)==0xc0u)val=b.psst[voice];
        else return false;
        b.events.push_back({'d',port,val});return true;
    }
    static bool out32(void* ptr,std::uint16_t port,std::uint32_t val) noexcept {
        auto& b=*static_cast<Bus*>(ptr);
        if(port!=0x0620u)return false;
        const auto voice=b.selector&31u;
        if((b.selector&0xe0u)==0xe0u)b.csl[voice]=val;
        else if((b.selector&0xe0u)==0xc0u)b.psst[voice]=val;
        else return false;
        b.events.push_back({'D',port,val});return true;
    }
    [[nodiscard]] PortIo io() noexcept {return {this,out8,out16,in16,in32,out32};}
};

static void w(Descriptor& d,unsigned at,std::uint16_t value) {
    d[at]=static_cast<std::uint8_t>(value);
    d[at+1]=static_cast<std::uint8_t>(value>>8u);
}
static DeviceState fresh() {
    DeviceState s;
    for(auto& v:s.voices) {v.key_marker=0x0ffe;v.sample_end=0;}
    return s;
}

struct FakeCore {
    bool open=true;
    std::array<std::uint16_t,65536> ports{};
    std::vector<Event> writes{};
    std::vector<std::uint16_t> reads{};
    [[nodiscard]] bool ready() const noexcept {return open;}
    void writePort(std::uint16_t p,std::uint16_t v) noexcept {
        ports[p]=v;writes.push_back({'w',p,v});
    }
    [[nodiscard]] std::uint16_t readPort(std::uint16_t p) noexcept {
        reads.push_back(p);return ports[p];
    }
};
int main() {
    // Real 125A Chip::writePort/readPort-compatible adapter ordering.
    {
        FakeCore core;
        auto p=ChipPortBridge<FakeCore>::bind(core);
        REQUIRE(p.out8(p.opaque,0xe22u,0x3au));
        REQUIRE(p.out32(p.opaque,0x620u,0x1234abcd));
        REQUIRE(core.writes.size()==3u);
        REQUIRE(core.writes[0].port==0xe22u && core.writes[0].value==0x3au);
        REQUIRE(core.writes[1].port==0x620u && core.writes[1].value==0xabcdu);
        REQUIRE(core.writes[2].port==0x622u && core.writes[2].value==0x1234u);
        std::uint32_t read=0;
        REQUIRE(p.in32(p.opaque,0x620u,read));
        REQUIRE(read==0x1234abcdu && core.reads.size()==2u);
        REQUIRE(core.reads[0]==0x620u && core.reads[1]==0x622u);
        core.open=false;
        REQUIRE(!p.out8(p.opaque,0xe22u,0));
        REQUIRE(!p.in32(p.opaque,0x620u,read));
    }
    // True Win95 LCOD+0x3EA1 MODE0 live search, not a score snapshot.
    {
        auto state=fresh();Bus bus;
        auto r=allocate_mono_mode0(state,bus.io());
        REQUIRE(r.status==Status::Found && r.first==0u && r.immediate);
        REQUIRE(r.io_reads==1u && r.io_writes==3u);
        REQUIRE(bus.events.size()==4u);
        REQUIRE(bus.events[0].kind=='b' && bus.events[0].value==0x60u);
        REQUIRE(bus.events[1].kind=='d' && bus.events[1].port==0x620u);
        REQUIRE(bus.events[2].value==0xa0u && bus.events[3].value==0xffu);
    }
    // Important: three interleaved passes, not 0,1,2,... consecutive.
    {
        auto state=fresh(); Bus bus;
        for(auto& v:state.voices)v.key_marker=0x1001u;
        bus.vtft.fill(0x10000000u);
        state.voices[3].key_marker=0x0ffeu; bus.vtft[3]=0u;
        auto r=allocate_mono_mode0(state,bus.io());
        REQUIRE(r.status==Status::Found && r.first==3u);
        REQUIRE(r.io_reads==3u); // vtft(0), sus(0), vtft(3) only
        REQUIRE(bus.events[0].value==0x60u);
        REQUIRE(bus.events[4].value==0x63u);
    }
    // If no free/finished voice, <= tie picks LAST row in lane-order.
    {
        auto state=fresh(); Bus bus;
        for(auto& v:state.voices)v.key_marker=0x1000u;
        for(auto& v:bus.vtft)v=0x09000000u;
        auto r=allocate_mono_mode0(state,bus.io());
        REQUIRE(r.status==Status::Found && r.first==29u);
        REQUIRE(!r.immediate);
    }
    // Blocked low-mode voice must not invoke hardware callbacks.
    {
        auto state=fresh();Bus bus;state.mode_count=2;
        state.voices[0].blocked=true;
        auto r=allocate_mono_mode0(state,bus.io());
        REQUIRE(r.status==Status::Found && r.first==3u);
        REQUIRE(bus.events[0].value==0x63u);
    }
    // Sample-end marker wins even with key marker >=0x2000.
    {
        auto state=fresh();Bus bus;
        state.physical_capacity=4;
        for(auto& v:state.voices)v.key_marker=0x2000u;
        state.voices[3].sample_end=32u;
        bus.ccca[3]=33u;
        auto r=allocate_mono_mode0(state,bus.io());
        REQUIRE(r.status==Status::Found && r.first==3u && r.immediate);
        REQUIRE(r.io_reads==1u);
    }
    // Source physical capacity 32 can select voice30 in its lane2 scan.
    // ROM/RAM musical reservation belongs to the caller's authenticated state.
    {
        auto state=fresh();Bus bus;
        for(auto& v:state.voices)v.key_marker=0x2000u;
        state.voices[30].key_marker=0x0ffeu;
        auto r=allocate_mono_mode0(state,bus.io());
        REQUIRE(r.status==Status::Found && r.first==30u);
        REQUIRE(bus.events[0].value==0x7eu && bus.events[2].value==0xbeu);
    }
    // The original no-candidate sentinel produces no hardware writes.
    {
        auto state=fresh();Bus bus;
        for(auto& v:state.voices)v.key_marker=0x2000u;
        auto r=allocate_mono_mode0(state,bus.io());
        REQUIRE(r.status==Status::NoCandidate && bus.events.empty());
    }
    // True LCOD+0x6072 / +0x60E4: NoteOff in original logical row order.
    {
        auto state=fresh();Bus bus;
        state.voices[0].key_marker=0x003cu; // channel 0, C4 (60)
        state.voices[0].volume_release=0x26u;
        state.voices[0].modulation_release=0x39u;
        state.voices[0].release_sample_end=0x001234u;
        bus.csl[0]=0xb7000100u;
        bus.psst[0]=0xcd000200u;
        const auto r=release_note(state,0,60,bus.io());
        REQUIRE(r.status==Status::Found && r.matched==1u &&
                r.released==1u && r.deferred==0u);
        REQUIRE(r.io_reads==2u && r.io_writes==10u);
        REQUIRE(state.voices[0].key_marker==0x0ffeu);
        REQUIRE(state.voices[0].sample_end==0x1234u);
        REQUIRE(bus.csl[0]==0xb7001238u && bus.psst[0]==0xcd001234u);
        REQUIRE(bus.events.size()==12u);
        REQUIRE(bus.events[0].value==0xa0u && bus.events[1].value==0x8026u);
        REQUIRE(bus.events[2].value==0xe0u && bus.events[3].value==0x8039u);
        REQUIRE(bus.events[4].value==0xe0u && bus.events[6].value==0xe0u);
        REQUIRE(bus.events[8].value==0xc0u);
    }
    // Original per-channel sustain defers release and emits NO chip I/O.
    {
        auto state=fresh();Bus bus;
        state.channel_sustain[2]=true;
        state.voices[3].key_marker=static_cast<std::uint16_t>((2u<<8u)|61u);
        auto r=release_note(state,2,61,bus.io());
        REQUIRE(r.status==Status::Found && r.deferred==1u && r.released==0u);
        REQUIRE(state.voices[3].key_marker==0x02ffu && bus.events.empty());
        REQUIRE(release_note(state,2,61,bus.io()).matched==0u);
    }
    // Actual LCOD+0x656D CC64 pedal-up scans deferred channel:FF rows.
    {
        auto state=fresh();Bus bus;
        state.voices[1].key_marker=0x01ffu;
        state.voices[29].key_marker=0x01ffu;
        state.voices[2].key_marker=0x02ffu; // different channel untouched
        state.channel_sustain[1]=true;
        auto r=controller_sustain(state,1,63,bus.io());
        REQUIRE(r.status==Status::Found && r.released==2u && r.matched==2u);
        REQUIRE(!state.channel_sustain[1]);
        REQUIRE(state.voices[1].key_marker==0x0ffeu);
        REQUIRE(state.voices[29].key_marker==0x0ffeu);
        REQUIRE(state.voices[2].key_marker==0x02ffu);
        REQUIRE(bus.events.size()==8u);
        r=controller_sustain(state,1,64,bus.io());
        REQUIRE(r.status==Status::Found && r.released==0u);
        REQUIRE(state.channel_sustain[1] && bus.events.size()==8u);
    }
    // One NoteOff can terminate two layered notes, including voice 30.
    {
        auto state=fresh();Bus bus;
        state.voices[29].key_marker=0x0040u;
        state.voices[30].key_marker=0x0040u;
        auto r=release_note(state,0,64,bus.io());
        REQUIRE(r.status==Status::Found && r.matched==2u && r.released==2u);
        REQUIRE(state.voices[29].key_marker==0x0ffeu);
        REQUIRE(state.voices[30].key_marker==0x0ffeu);
        REQUIRE(bus.events.size()==8u);
        REQUIRE(bus.events[0].value==0xbdu);
        REQUIRE(bus.events[4].value==0xbeu);
    }
    // No voice mutation on invalid channel, key or absent required out32.
    {
        auto state=fresh();Bus bus;
        state.voices[0].key_marker=0x003cu;
        PortIo io=bus.io();io.out32=nullptr;
        REQUIRE(release_note(state,0,60,io).status==Status::MissingSource);
        REQUIRE(state.voices[0].key_marker==0x003cu);
        // True sustain branch is valid even with no chip callback.
        state.channel_sustain[0]=true;
        REQUIRE(release_note(state,0,60,io).deferred==1u);
        REQUIRE(state.voices[0].key_marker==0x00ffu);
        REQUIRE(controller_sustain(state,0,65,io).status==Status::Found);
        REQUIRE(state.channel_sustain[0]);
        REQUIRE(release_note(state,16,60,bus.io()).status==Status::MissingSource);
        REQUIRE(release_note(state,0,128,bus.io()).status==Status::MissingSource);
    }
    // The actual owner V4 captured physical pair 29/30. Force that pair
    // by making voice31 costly and voice29/30 absolutely idle.
    {
        auto state=fresh();Bus bus;
        bus.vtft[31]=static_cast<std::uint32_t>(60000u)<<16u;
        auto r=allocate_pair_mode1(state,bus.io());
        REQUIRE(r.status==Status::Found);
        REQUIRE(r.first==29u && r.second==30u);
        REQUIRE(r.immediate && r.priority==0u);
        REQUIRE(r.io_reads==3u);
        REQUIRE(bus.events.size()==10u); // 3 E22 + 3 DWORD + 2*(E22+OUTW)
        REQUIRE(bus.events[0].kind=='b' && bus.events[0].value==0x7fu);
        REQUIRE(bus.events[2].value==0x7eu);
        REQUIRE(bus.events[4].value==0x7du);
        REQUIRE(bus.events[6].port==0x0e22u && bus.events[6].value==0xbdu);
        REQUIRE(bus.events[7].port==0x0a20u && bus.events[7].value==0xffu);
        REQUIRE(bus.events[8].value==0xbeu); // second release voice30
    }
    // All idle => first adjacent pair 30/31 (physical capacity, not mono cap).
    {
        auto state=fresh();Bus bus;
        auto r=allocate_pair_mode1(state,bus.io());
        REQUIRE(r.status==Status::Found && r.first==30u && r.second==31u);
        REQUIRE(r.immediate);
    }
    // Blocked voice skipped before any MMIO. Low mode_count is mutable!
    {
        auto state=fresh();state.mode_count=2;state.voices[31].blocked=true;
        Bus bus;auto r=allocate_pair_mode1(state,bus.io());
        REQUIRE(r.status==Status::Found && r.first==28u && r.second==29u);
        for(auto e:bus.events)REQUIRE(!(e.port==0x0e22u && e.value==0x7fu));
    }
    // Exhaustion marker >=0x2000; no speculative chip I/O.
    {
        auto state=fresh();Bus bus;
        for(auto& v:state.voices)v.key_marker=0x2000;
        auto r=allocate_pair_mode1(state,bus.io());
        REQUIRE(r.status==Status::NoCandidate && r.first==no_voice);
        REQUIRE(bus.events.empty());
    }
    // Score, live ENV and sample end, original DCYSUSV/CCCA port reads.
    {
        auto state=fresh(); state.physical_capacity=2;Bus bus;
        for(auto& v:state.voices)v.key_marker=0x1101;
        bus.vtft[0]=0x06000000u;bus.vtft[1]=0x05000000u;
        bus.sus[0]=0;bus.sus[1]=0x8000u;
        auto r=allocate_pair_mode1(state,bus.io());
        REQUIRE(r.status==Status::Found && r.first==0 && r.second==1);
        REQUIRE(!r.immediate && r.io_reads==4u);
        REQUIRE(r.priority>0);
    }
    {
        auto state=fresh(); state.physical_capacity=2;Bus bus;
        state.voices[0].key_marker=0x1101;
        state.voices[0].sample_end=100u;bus.ccca[0]=100u;
        auto r=allocate_pair_mode1(state,bus.io());
        REQUIRE(r.status==Status::Found && r.first==0);
        REQUIRE(r.io_reads>=3u);
    }
    // Port errors cannot become successful allocations.
    {
        auto state=fresh();Bus bus;bus.fail_next_read=true;
        auto r=allocate_pair_mode1(state,bus.io());
        REQUIRE(r.status==Status::PortError);
        REQUIRE(r.first==no_voice);
    }
    {
        auto state=fresh();Bus bus;PortIo missing=bus.io();missing.in32=nullptr;
        auto r=allocate_pair_mode1(state,missing);
        REQUIRE(r.status==Status::MissingSource);
    }
    // Mode2 exact exclusive bounds and ascending selection, including 30/31.
    {
        auto state=fresh();Bus bus;state.mode_count=1;
        state.voices[29].blocked=true;
        auto r=allocate_range_mode2(state,(32u<<8u)|29u,bus.io());
        REQUIRE(r.status==Status::Found && r.first==30u && r.second==no_voice);
        REQUIRE(r.immediate);
        for(auto e:bus.events)REQUIRE(!(e.port==0x0e22u && e.value==0x7du));
    }
    {
        auto state=fresh();Bus bus;
        auto r=allocate_range_mode2(state,(30u<<8u)|30u,bus.io());
        REQUIRE(r.status==Status::NoCandidate && bus.events.empty());
    }
    {
        auto state=fresh();Bus bus;
        state.voices[0].key_marker=0x2000;
        state.voices[0].sample_end=40;
        bus.ccca[0]=50;
        auto r=allocate_range_mode2(state,(1u<<8u)|0u,bus.io());
        REQUIRE(r.status==Status::Found && r.first==0u && r.immediate);
        REQUIRE(r.io_reads==1u);
    }
    // Original stereo worklist: partner(L) first, trigger(R) second.
    {
        std::array<Descriptor,3> d{};
        w(d[0],0x6a,8u);w(d[0],0x8c,0x8002u);w(d[0],0x8e,9u); // R
        w(d[1],0x6a,9u);w(d[1],0x8c,0x8004u);w(d[1],0x8e,8u); // L
        w(d[2],0x6a,11u);w(d[2],0x8c,1u); // independent
        const auto initial=d;
        auto list=original_stereo_worklist(d);
        REQUIRE(list.status==WorkStatus::Ok && list.count==3 && list.stereo_pairs==1);
        REQUIRE(list.items[0].source_index==1 && list.items[0].process_type==4);
        REQUIRE(list.items[1].source_index==0 && list.items[1].process_type==2);
        REQUIRE(list.items[2].source_index==2 && list.items[2].process_type==1);
        REQUIRE(d==initial); // do not mutate user SF2 data
        PhysicalWorkItem first{},second{};
        auto selected=Allocation{Status::Found,29,30};
        REQUIRE(original_assign_stereo_pair(list.items[0],list.items[1],selected,first,second));
        REQUIRE(first.physical_voice==30 && second.physical_voice==29);
        selected.second=31;
        REQUIRE(!original_assign_stereo_pair(list.items[0],list.items[1],selected,first,second));
    }
    // No matching ID: original second-pass mono fallback, NOT fake stereo.
    {
        std::array<Descriptor,1> d{};
        w(d[0],0x6a,9);w(d[0],0x8c,2);w(d[0],0x8e,12);
        auto r=original_stereo_worklist(d);
        REQUIRE(r.status==WorkStatus::Ok && r.stereo_pairs==0 &&
                r.items[0].process_type==1);
    }
    // Source worklist bounded in a deterministic, RT-safe manner.
    {
        std::array<Descriptor,max_note_descriptors+1> oversized{};
        REQUIRE(original_stereo_worklist(oversized).status==WorkStatus::TooManyDescriptors);
        REQUIRE(original_stereo_worklist({}).status==WorkStatus::BadInput);
    }
    std::puts("125A_CLEANROOM_CREATIVE_LIVE_MODE1_MODE2_STEREO_CPP23_PASS");
}
