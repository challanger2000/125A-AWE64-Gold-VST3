#include "OriginalCreativeSf2RamUpload.hpp"
#include "OriginalCreativeRamChipSink.hpp"
#include "OriginalCreativeRamScratchVoices.hpp"
#include "OriginalCreativeChipBridge.hpp"
#include "OriginalCreativeMidiRamRuntime.hpp"
#include <array>
#include <algorithm>
#include <atomic>
#include <new>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace awe64::original_vxd;
#define REQUIRE(...) do{if(!(__VA_ARGS__)){std::fprintf(stderr,"SF2_READER_FAIL line %d: %s\n",__LINE__,#__VA_ARGS__);std::exit(1);}}while(false)
namespace {
std::atomic<bool> measure_ram_upload_heap{false};
std::atomic<std::uint64_t> measured_ram_upload_allocations{0u};
}
void* operator new(std::size_t n) {
    if(measure_ram_upload_heap.load(std::memory_order_relaxed))
        measured_ram_upload_allocations.fetch_add(1u,std::memory_order_relaxed);
    void* p=std::malloc(n?n:1u);
    if(!p)throw std::bad_alloc();
    return p;
}
void* operator new[](std::size_t n) {
    if(measure_ram_upload_heap.load(std::memory_order_relaxed))
        measured_ram_upload_allocations.fetch_add(1u,std::memory_order_relaxed);
    void* p=std::malloc(n?n:1u);
    if(!p)throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
using B=std::vector<std::uint8_t>;
static void u16(B& b,std::uint16_t v){b.push_back(static_cast<std::uint8_t>(v));b.push_back(static_cast<std::uint8_t>(v>>8));}
static void u32(B& b,std::uint32_t v){u16(b,static_cast<std::uint16_t>(v));u16(b,static_cast<std::uint16_t>(v>>16));}
static void chunk(B& b,const char (&name)[5],B const& d){for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(name[i]));u32(b,static_cast<std::uint32_t>(d.size()));b.insert(b.end(),d.begin(),d.end());if(d.size()&1u)b.push_back(0u);}
static void list(B& b,const char (&name)[5],B const& d){B x;for(unsigned i=0;i<4;++i)x.push_back(static_cast<std::uint8_t>(name[i]));x.insert(x.end(),d.begin(),d.end());chunk(b,"LIST",x);}
static void phdr(B& b,std::uint16_t program,std::uint16_t bank,std::uint16_t bag){b.insert(b.end(),20u,0u);u16(b,program);u16(b,bank);u16(b,bag);b.insert(b.end(),12u,0u);}
static void inst(B& b,std::uint16_t bag){b.insert(b.end(),20u,0u);u16(b,bag);}
static void bag(B& b,std::uint16_t gen,std::uint16_t mod=0u){u16(b,gen);u16(b,mod);}
static void gen(B& b,std::uint16_t op,std::uint16_t amount){u16(b,op);u16(b,amount);}
static void sample(B& b,std::uint32_t start,std::uint32_t end,std::uint32_t ls,std::uint32_t le,std::uint16_t link,std::uint16_t kind){b.insert(b.end(),20u,0u);u32(b,start);u32(b,end);u32(b,ls);u32(b,le);u32(b,44100u);b.push_back(60u);b.push_back(0u);u16(b,link);u16(b,kind);}
struct Fixture {B file{};std::size_t pbag_offset=0u;std::size_t igen_offset=0u;std::size_t smpl_offset=0u;};
static Fixture make_fixture(std::uint16_t sampleTypeA=2u,std::uint16_t sampleTypeB=4u,bool modulated=false,std::size_t sample_words=1200u){
    B info,ifil;u16(ifil,2);u16(ifil,1);chunk(info,"ifil",ifil);
    B pd,ph,pb,pg,in,ib,ig,im,sh,sampleData(sample_words*2u,0u);
    phdr(ph,7u,1u,0u);phdr(ph,0u,0u,2u);
    bag(pb,0u);bag(pb,1u);bag(pb,3u);
    gen(pg,8u,100u);gen(pg,43u,static_cast<std::uint16_t>((65u<<8u)|60u));gen(pg,41u,0u);
    inst(in,0u);inst(in,3u);
    bag(ib,0u,0u);bag(ib,1u,0u);bag(ib,4u,modulated?1u:0u);bag(ib,7u,modulated?1u:0u);
    gen(ig,8u,88u);
    gen(ig,43u,static_cast<std::uint16_t>((64u<<8u)|60u));
    gen(ig,44u,static_cast<std::uint16_t>((127u<<8u)|1u));
    gen(ig,53u,0u);
    gen(ig,43u,static_cast<std::uint16_t>((64u<<8u)|60u));
    gen(ig,44u,static_cast<std::uint16_t>((127u<<8u)|1u));
    gen(ig,53u,1u);
    sample(sh,100u,300u,150u,250u,1u,sampleTypeA);
    sample(sh,400u,600u,450u,550u,0u,sampleTypeB);
    sample(sh,1200u,1200u,1200u,1200u,0u,1u);
    chunk(pd,"phdr",ph);chunk(pd,"pbag",pb);chunk(pd,"pgen",pg);
    if(modulated) {
        // One 10-byte instrument modulator for the first zone, followed
        // by an ordinary terminal imod record.
        u16(im,2u);u16(im,8u);u16(im,50u);u16(im,0u);u16(im,0u);
    }
    im.insert(im.end(),10u,0u);
    chunk(pd,"inst",in);chunk(pd,"ibag",ib);chunk(pd,"igen",ig);
    chunk(pd,"imod",im);
    chunk(pd,"shdr",sh);
    B sd;chunk(sd,"smpl",sampleData);
    B body;for(char c:"sfbk"){if(c)body.push_back(static_cast<std::uint8_t>(c));}
    list(body,"INFO",info);list(body,"sdta",sd);list(body,"pdta",pd);
    Fixture fixture;for(char c:"RIFF"){if(c)fixture.file.push_back(static_cast<std::uint8_t>(c));}
    u32(fixture.file,static_cast<std::uint32_t>(body.size()));
    fixture.file.insert(fixture.file.end(),body.begin(),body.end());return fixture;
}

struct OriginalPortChip {
    bool initialized=true;
    std::size_t count=0u;
    std::array<std::pair<std::uint16_t,std::uint16_t>,4096u> events{};
    std::size_t reads=0u;
    [[nodiscard]] bool ready() const noexcept {return initialized;}
    [[nodiscard]] std::uint16_t readPort(std::uint16_t) noexcept {++reads; return 0u;}
    void writePort(const std::uint16_t port,const std::uint16_t data) noexcept {
        if(count>=events.size())std::abort();
        events[count++]={port,data};
    }
};
// Constant-memory full-bank sink: does not retain or replay a register trace.
struct CountingPortChip {
    std::uint64_t writes=0u, reads=0u;
    std::uint64_t fail_after_writes=0u;
    [[nodiscard]] bool ready() const noexcept {
        return fail_after_writes==0u || writes<fail_after_writes;
    }
    void writePort(std::uint16_t,std::uint16_t) noexcept {++writes;}
    [[nodiscard]] std::uint16_t readPort(std::uint16_t) noexcept {
        ++reads;return 0u;
    }
};
struct RamSink {
    std::array<std::int16_t,1200u> data{};
    std::uint32_t first=0x200000u;
    std::size_t words=0u,calls=0u;
    std::size_t fail_on=0u;
    static bool upload(void* opaque,std::uint32_t addr,
                       std::span<const std::int16_t> bytes) noexcept {
        auto& self=*static_cast<RamSink*>(opaque);
        if(self.fail_on && self.calls+1u==self.fail_on)return false;
        if(addr!=self.first+self.words || self.words+bytes.size()>self.data.size())
            return false;
        for(auto w:bytes)self.data[self.words++]=w;
        ++self.calls;
        return true;
    }
    OriginalSf2RamSink writer() noexcept {return {this,upload};}
};
struct OriginalRamSink {
    std::uint32_t expected_bank_base=0x200000u;
    std::size_t source_words=0u,call_index=0u;
    std::size_t accepted_words=0u;
    std::size_t fail_at=0u;
    std::array<std::int16_t,1300u> small_capture{};
    [[nodiscard]] static bool upload(void* ptr,std::uint32_t addr,
                                    std::span<const std::int16_t> words) noexcept {
        auto& s=*static_cast<OriginalRamSink*>(ptr);
        if(s.fail_at!=0u && s.call_index+1u==s.fail_at)return false;
        const auto count=(s.source_words+1023u)/1024u;
        const auto index=s.call_index++;
        const std::uint32_t expected_addr=index==0u?s.expected_bank_base:
            index<count?s.expected_bank_base+14u+static_cast<std::uint32_t>(index*1024u):
            s.expected_bank_base+14u+static_cast<std::uint32_t>(s.source_words);
        const auto expected_length=index<count?
            (index==0u?14u:0u)+std::min<std::size_t>(
                1024u,s.source_words-index*1024u)+1u:17u;
        if(addr!=expected_addr || words.size()!=expected_length)return false;
        if(index<count && words.back()!=0)return false;
        if(index==count)for(auto v:words)if(v!=0)return false;
        if(index==0u){
            for(unsigned i=0;i<14u;++i)if(words[i]!=0)return false;
            const auto amount=std::min(words.size(),s.small_capture.size());
            for(std::size_t i=0;i<amount;++i)s.small_capture[i]=words[i];
        }
        s.accepted_words+=words.size();
        return true;
    }
    OriginalSf2RamSink writer() noexcept{return {this,upload};}
};
int main(){
    auto f=make_fixture();
    bool found=false;
    for(std::size_t pos=0;pos+20u<f.file.size();++pos) {
        if(f.file[pos]=='s' && f.file[pos+1u]=='m' &&
           f.file[pos+2u]=='p' && f.file[pos+3u]=='l') {
            // Raw PCM16 little-endian: +0x1234, -32767, -1.
            const auto at=pos+8u;
            f.file[at]=0x34u;f.file[at+1u]=0x12u;
            f.file[at+2u]=1u;f.file[at+3u]=0x80u;
            f.file[at+4u]=0xffu;f.file[at+5u]=0xffu;
            found=true;break;
        }
    }
    REQUIRE(found);
    RamSink sink{};
    OriginalSf2RamUploadSource source{true,0x200000u};
    auto status=upload_verified_sf2_pcm_to_emu8000(f.file,source,sink.writer());
    REQUIRE(status.status==OriginalSf2UploadStatus::Ok);
    REQUIRE(status.words_written==1200u && status.callback_count==5u);
    REQUIRE(sink.calls==5u && sink.words==1200u);
    REQUIRE(sink.data[0]==0x1234 && sink.data[1]==-32767 && sink.data[2]==-1);
    REQUIRE(sink.data[1199]==0);
    sink=RamSink{};source.original_guest_ram_base_verified=false;
    status=upload_verified_sf2_pcm_to_emu8000(f.file,source,sink.writer());
    REQUIRE(status.status==OriginalSf2UploadStatus::SourceMissing && sink.calls==0u);
    source.original_guest_ram_base_verified=true;
    source.original_physical_word_base=0x1000000u-1199u;
    status=upload_verified_sf2_pcm_to_emu8000(f.file,source,sink.writer());
    REQUIRE(status.status==OriginalSf2UploadStatus::AddressOutOfRange && sink.calls==0u);
    source.original_physical_word_base=0x200000u;
    sink.fail_on=2u;
    status=upload_verified_sf2_pcm_to_emu8000(f.file,source,sink.writer());
    REQUIRE(status.status==OriginalSf2UploadStatus::UploadFailed);
    REQUIRE(status.words_written==256u && status.callback_count==1u);
    sink=RamSink{};
    auto bad=f.file;
    for(std::size_t i=0;i+4u<bad.size();++i) {
        if(bad[i]=='s'&&bad[i+1u]=='m'&&bad[i+2u]=='p'&&bad[i+3u]=='l'){
            bad[i]='x';break;
        }
    }
    status=upload_verified_sf2_pcm_to_emu8000(bad,source,sink.writer());
    REQUIRE(status.status==OriginalSf2UploadStatus::MalformedBank && sink.calls==0u);
    // Independent x86 PCOD+0x28AC regression, including early-zero branch
    // and signed arithmetic right shift (not a naive 6-tap or float filter).
    OriginalCreativePcmFilter filter{};
    constexpr std::array<std::int16_t,19> raw{{
        0,12,64,-160,32767,-32768,0,0,0,0,-1,1,0,0,1024,2048,0,0,0}};
    constexpr std::array<std::int16_t,19> expected{{
        0,0,0,1,66,-4224,28691,-28672,0,0,0,0,-1,0,-1,-128,512,1408,0}};
    for(std::size_t i=0;i<raw.size();++i)
        REQUIRE(filter.next(raw[i])==expected[i]);

    // The REAL Creative uploader is deliberately a separate path from the
    // previous raw-PCM diagnostic function above. It prepends 14 zeros,
    // transforms every source sample, inserts one zero per 1024-word block
    // and writes a final 17 zeros; the allocator reserves only +16 words.
    auto authentic=make_fixture(2u,4u,false,1200u);
    bool updated=false;
    for(std::size_t j=0;j+8u<authentic.file.size();++j){
        if(authentic.file[j]=='s' && authentic.file[j+1u]=='m' &&
           authentic.file[j+2u]=='p' && authentic.file[j+3u]=='l'){
            for(std::size_t i=0;i<raw.size();++i){
                const auto v=static_cast<std::uint16_t>(raw[i]);
                authentic.file[j+8u+2u*i]=static_cast<std::uint8_t>(v);
                authentic.file[j+9u+2u*i]=static_cast<std::uint8_t>(v>>8u);
            }
            updated=true;break;
        }
    }
    REQUIRE(updated);
    OriginalCreativeRamBankCursor cursor{};
    OriginalRamSink original_sink{};
    original_sink.source_words=1200u;
    auto original_status=upload_original_creative_sf2_ram_bank(
        authentic.file,cursor,original_sink.writer());
    REQUIRE(original_status.status==OriginalSf2UploadStatus::SourceMissing &&
            original_sink.call_index==0u);
    cursor.original_28mib_device_verified=true;
    original_status=upload_original_creative_sf2_ram_bank(
        authentic.file,cursor,original_sink.writer());
    REQUIRE(original_status.status==OriginalSf2UploadStatus::Ok);
    REQUIRE(original_status.placement.allocation_start_word==0x200000u);
    REQUIRE(original_status.placement.first_sample_word==0x20000eu);
    REQUIRE(original_status.placement.next_bank_word==0x2004c0u);
    REQUIRE(original_status.source_pcm_words==1200u);
    REQUIRE(original_status.physical_sample_writes==1233u);
    REQUIRE(original_status.transfer_chunks==3u);
    REQUIRE(original_sink.call_index==3u && original_sink.accepted_words==1233u);
    for(std::size_t i=0;i<raw.size();++i)
        REQUIRE(original_sink.small_capture[14u+i]==expected[i]);
    // End-to-end: actual original-Creative source PCM -> x86 filter ->
    // original SMALW/SMLD hardware port ordering, including two blocks,
    // the overlapped dummy zero, and the final 17-word guard transfer.
    // This is a fake Chip adapter contract test, not a VST3/EMU DSP test.
    OriginalCreativeRamBankCursor port_cursor{true,false,0x200000u};
    OriginalPortChip real_port_adapter{};
    const auto port_result=upload_original_creative_sf2_ram_bank(
        authentic.file,port_cursor,
        OriginalCreativeRamChipSink<OriginalPortChip>::bind(real_port_adapter));
    REQUIRE(port_result.status==OriginalSf2UploadStatus::Ok &&
            real_port_adapter.count==2u*1233u+3u*3u);
    // Start of the first transfer: select 0x36, SMALW low/high,
    // then re-select 0x3A before EVERY data word, including guards.
    REQUIRE(real_port_adapter.events[0]==std::pair<std::uint16_t,std::uint16_t>(0xe22u,0x36u));
    REQUIRE(real_port_adapter.events[1]==std::pair<std::uint16_t,std::uint16_t>(0xa20u,0u));
    REQUIRE(real_port_adapter.events[2]==std::pair<std::uint16_t,std::uint16_t>(0xa22u,0x20u));
    REQUIRE(real_port_adapter.events[3]==std::pair<std::uint16_t,std::uint16_t>(0xe22u,0x3au));
    for(unsigned i=0;i<14u;++i) {
        REQUIRE(real_port_adapter.events[3u+2u*i].first==0xe22u &&
                real_port_adapter.events[3u+2u*i].second==0x3au);
        REQUIRE(real_port_adapter.events[4u+2u*i].second==0u);
    }
    for(unsigned i=0;i<expected.size();++i) {
        REQUIRE(real_port_adapter.events[3u+2u*(14u+i)].second==0x3au);
        REQUIRE(real_port_adapter.events[4u+2u*(14u+i)].second==
                static_cast<std::uint16_t>(expected[i]));
    }
    const std::size_t second_port_block=3u+2u*(14u+1024u+1u);
    REQUIRE(real_port_adapter.events[second_port_block]==
            std::pair<std::uint16_t,std::uint16_t>(0xe22u,0x36u));
    REQUIRE(real_port_adapter.events[second_port_block+1u].second==0x040eu);
    REQUIRE(real_port_adapter.events[second_port_block+2u].second==0x0020u);
    REQUIRE(real_port_adapter.events[second_port_block+3u]==
            std::pair<std::uint16_t,std::uint16_t>(0xe22u,0x3au));
    const auto final_offset=second_port_block+3u+2u*((1200u-1024u)+1u);
    REQUIRE(real_port_adapter.events[final_offset]==
            std::pair<std::uint16_t,std::uint16_t>(0xe22u,0x36u));
    REQUIRE(real_port_adapter.events[final_offset+1u].second==0x04beu);
    REQUIRE(real_port_adapter.events[final_offset+2u].second==0x0020u);
    for(std::size_t i=0;i<17u;++i) {
        REQUIRE(real_port_adapter.events[final_offset+3u+i*2u]==
                std::pair<std::uint16_t,std::uint16_t>(0xe22u,0x3au));
        REQUIRE(real_port_adapter.events[final_offset+4u+i*2u]==
                std::pair<std::uint16_t,std::uint16_t>(0xa20u,0u));
    }
    REQUIRE(port_cursor.next_bank_word==0x2004c0u);
    // One-call functional C++23 end-to-end: live ten-voice allocator,
    // scratch register writes, genuine PCM transformation and SMALW/SMLD.
    // The mock's actual port reads return zero, as in original Win95 bank1.
    // No previously captured register or voice-number list is replayed.
    OriginalPortChip integrated_chip{};
    DeviceState integrated_state{};
    integrated_state.physical_capacity=32u;
    integrated_state.mode_count=32u;
    OriginalCreativeRamScratchCycle integrated_scratch{};
    integrated_scratch.device=&integrated_state;
    integrated_scratch.io=ChipPortBridge<OriginalPortChip>::bind(integrated_chip);
    OriginalCreativeRamBankCursor integrated_cursor{true,false,0x200000u};
    measured_ram_upload_allocations.store(0u,std::memory_order_relaxed);
    measure_ram_upload_heap.store(true,std::memory_order_relaxed);
    const auto integrated_status=upload_original_creative_sf2_ram_bank(
        authentic.file,integrated_cursor,
        OriginalCreativeRamChipSink<OriginalPortChip>::bind(integrated_chip),
        integrated_scratch.hooks());
    measure_ram_upload_heap.store(false,std::memory_order_relaxed);
    REQUIRE(integrated_status.status==OriginalSf2UploadStatus::Ok);
    REQUIRE(measured_ram_upload_allocations.load(std::memory_order_relaxed)==0u);
    REQUIRE(integrated_status.physical_sample_writes==1233u);
    REQUIRE(integrated_status.transfer_chunks==3u);
    REQUIRE(integrated_chip.count==2u*1233u+3u*3u+3u*410u);
    REQUIRE(integrated_chip.reads==60u);
    REQUIRE(integrated_scratch.cycles_completed==3u &&
            integrated_scratch.selected_voices==30u &&
            !integrated_scratch.active && !integrated_scratch.faulted);
    REQUIRE(integrated_cursor.next_bank_word==0x2004c0u);
    REQUIRE(integrated_state.mode_count==42u);
    // Same complete genuine source path via the new shared MIDI/RAM
    // runtime, not a second privately initialized DeviceState. This
    // prevents losing MIDI-occupied voices before later bank uploads.
    OriginalPortChip session_chip{};
    OriginalCreativeMidiRamRuntime session{};
    REQUIRE(session.bind_chip(session_chip));
    REQUIRE(session.attached_to(session_chip));
    OriginalCreativeRamBankCursor session_cursor{true,false,0x200000u};
    const auto session_upload=session.upload_bank(
        session_chip,authentic.file,session_cursor);
    REQUIRE(session_upload.status==OriginalSf2UploadStatus::Ok);
    REQUIRE(session_chip.count==2u*1233u+3u*3u+3u*410u);
    REQUIRE(session_chip.reads==60u);
    REQUIRE(session.scratch.cycles_completed==3u &&
            session.scratch.device==&session.midi.device);
    REQUIRE(session_cursor.next_bank_word==0x2004c0u);
    REQUIRE(!session.midi.fault && !session_cursor.faulted);
    for(unsigned v=0;v<30u;v+=3u) {
        REQUIRE(integrated_state.voices[v].key_marker==0x0ffeu);
        REQUIRE(integrated_state.voices[v].blocked);
    }
    // Enforce the integration boundary: no partial callbacks are legal.
    OriginalCreativeRamBankCursor bad_hook_cursor{true,false,0x200000u};
    OriginalPortChip bad_hook_chip{};
    OriginalCreativeRamBlockHooks missing_after=integrated_scratch.hooks();
    missing_after.after_block=nullptr;
    const auto bad_hook_result=upload_original_creative_sf2_ram_bank(
        authentic.file,bad_hook_cursor,
        OriginalCreativeRamChipSink<OriginalPortChip>::bind(bad_hook_chip),
        missing_after);
    REQUIRE(bad_hook_result.status==OriginalSf2UploadStatus::SourceMissing);
    REQUIRE(!bad_hook_cursor.faulted && bad_hook_chip.count==0u);
    // Even if the first physical I/O fails, latch the incomplete
    // transport and never claim that the EMU8000 state was rolled back.
    OriginalPortChip failed_scratch_chip{};
    failed_scratch_chip.initialized=false;
    DeviceState failed_scratch_state{};
    OriginalCreativeRamScratchCycle failed_scratch{};
    failed_scratch.device=&failed_scratch_state;
    failed_scratch.io=ChipPortBridge<OriginalPortChip>::bind(failed_scratch_chip);
    OriginalCreativeRamBankCursor failed_scratch_cursor{true,false,0x200000u};
    const auto failed_scratch_upload=upload_original_creative_sf2_ram_bank(
        authentic.file,failed_scratch_cursor,
        OriginalCreativeRamChipSink<OriginalPortChip>::bind(failed_scratch_chip),
        failed_scratch.hooks());
    REQUIRE(failed_scratch_upload.status==OriginalSf2UploadStatus::UploadFailed);
    REQUIRE(failed_scratch_cursor.faulted && failed_scratch.faulted);
    REQUIRE(failed_scratch_chip.count==0u &&
            failed_scratch_cursor.next_bank_word==0x200000u);
    // Offline-only guard: if Chip is not ready, the uploader must NOT
    // advance bank placement, change voice state or emit any port write.
    port_cursor=OriginalCreativeRamBankCursor{true,false,0x200000u};
    real_port_adapter=OriginalPortChip{};
    real_port_adapter.initialized=false;
    const auto rejected=upload_original_creative_sf2_ram_bank(
        authentic.file,port_cursor,
        OriginalCreativeRamChipSink<OriginalPortChip>::bind(real_port_adapter));
    REQUIRE(rejected.status==OriginalSf2UploadStatus::UploadFailed &&
            port_cursor.faulted && real_port_adapter.count==0u &&
            port_cursor.next_bank_word==0x200000u);
    // An absent bank must not be silently replaced with the previous one.
    auto corrupted=authentic.file;corrupted[0u]=0u;
    const auto previous=cursor.next_bank_word;
    original_status=upload_original_creative_sf2_ram_bank(
        corrupted,cursor,original_sink.writer());
    REQUIRE(original_status.status==OriginalSf2UploadStatus::MalformedBank &&
            cursor.next_bank_word==previous);
    OriginalRamSink second_sink{};
    second_sink.expected_bank_base=previous;
    second_sink.source_words=1200u;
    original_status=upload_original_creative_sf2_ram_bank(
        authentic.file,cursor,second_sink.writer());
    REQUIRE(original_status.status==OriginalSf2UploadStatus::Ok &&
            original_status.placement.first_sample_word==previous+14u &&
            cursor.next_bank_word==previous+1216u);
    // Original two-bank allocation sizes measured with guest RAM APIs.
    // This fixture is all synthetic zero PCM, not copied Creative audio.
    cursor=OriginalCreativeRamBankCursor{true,false,0x200000u};
    auto first_big=make_fixture(2u,4u,false,260921u);
    auto second_big=make_fixture(2u,4u,false,65536u);
    OriginalRamSink first_big_sink{};first_big_sink.source_words=260921u;
    measured_ram_upload_allocations.store(0u,std::memory_order_relaxed);
    measure_ram_upload_heap.store(true,std::memory_order_relaxed);
    original_status=upload_original_creative_sf2_ram_bank(
        first_big.file,cursor,first_big_sink.writer());
    measure_ram_upload_heap.store(false,std::memory_order_relaxed);
    REQUIRE(measured_ram_upload_allocations.load(std::memory_order_relaxed)==0u);
    REQUIRE(original_status.status==OriginalSf2UploadStatus::Ok &&
            original_status.physical_sample_writes==261207u &&
            original_status.transfer_chunks==256u &&
            cursor.next_bank_word==0x23fb49u);
    OriginalRamSink second_big_sink{};
    second_big_sink.expected_bank_base=0x23fb49u;
    second_big_sink.source_words=65536u;
    measure_ram_upload_heap.store(true,std::memory_order_relaxed);
    original_status=upload_original_creative_sf2_ram_bank(
        second_big.file,cursor,second_big_sink.writer());
    measure_ram_upload_heap.store(false,std::memory_order_relaxed);
    REQUIRE(measured_ram_upload_allocations.load(std::memory_order_relaxed)==0u);
    REQUIRE(original_status.status==OriginalSf2UploadStatus::Ok &&
            original_status.physical_sample_writes==65631u &&
            original_status.transfer_chunks==65u &&
            original_status.placement.first_sample_word==0x23fb57u &&
            cursor.next_bank_word==0x24fb59u);
    // Whole 260,921-word original-bank-size synthetic integration: the
    // source allocator and chip-IO bridge really execute all 256 scratch
    // cycles. This probes the very same event totals as the authenticated
    // independent owner-bank trace, but does not use its copyrighted PCM.
    // The destination counter retains no port events and allocates no heap.
    CountingPortChip full_port{};
    DeviceState full_state{};
    OriginalCreativeRamScratchCycle full_scratch{};
    full_scratch.device=&full_state;
    full_scratch.io=ChipPortBridge<CountingPortChip>::bind(full_port);
    OriginalCreativeRamBankCursor full_cursor{true,false,0x200000u};
    measured_ram_upload_allocations.store(0u,std::memory_order_relaxed);
    measure_ram_upload_heap.store(true,std::memory_order_relaxed);
    const auto full_result=upload_original_creative_sf2_ram_bank(
        first_big.file,full_cursor,
        OriginalCreativeRamChipSink<CountingPortChip>::bind(full_port),
        full_scratch.hooks());
    measure_ram_upload_heap.store(false,std::memory_order_relaxed);
    REQUIRE(full_result.status==OriginalSf2UploadStatus::Ok);
    REQUIRE(measured_ram_upload_allocations.load(std::memory_order_relaxed)==0u);
    REQUIRE(full_result.transfer_chunks==256u &&
            full_result.physical_sample_writes==261207u);
    REQUIRE(full_port.writes==628142u && full_port.reads==5120u);
    REQUIRE(full_scratch.cycles_completed==256u &&
            full_scratch.selected_voices==2560u &&
            !full_scratch.faulted && !full_scratch.active);
    REQUIRE(full_state.mode_count==42u);
    REQUIRE(full_cursor.next_bank_word==0x23fb49u && !full_cursor.faulted);
    // Native second-bank chain without resetting source device/allocator.
    // This proves bounds, 2-bank cursor accounting, and heap freedom;
    // it is deliberately NOT the occupied-voice Win95 stress differential.
    measured_ram_upload_allocations.store(0u,std::memory_order_relaxed);
    measure_ram_upload_heap.store(true,std::memory_order_relaxed);
    const auto second_full=upload_original_creative_sf2_ram_bank(
        second_big.file,full_cursor,
        OriginalCreativeRamChipSink<CountingPortChip>::bind(full_port),
        full_scratch.hooks());
    measure_ram_upload_heap.store(false,std::memory_order_relaxed);
    REQUIRE(second_full.status==OriginalSf2UploadStatus::Ok);
    REQUIRE(measured_ram_upload_allocations.load(std::memory_order_relaxed)==0u);
    REQUIRE(second_full.transfer_chunks==65u &&
            second_full.physical_sample_writes==65631u);
    REQUIRE(full_port.writes==786249u && full_port.reads==6420u);
    REQUIRE(full_scratch.cycles_completed==321u &&
            full_scratch.selected_voices==3210u &&
            !full_scratch.active && !full_scratch.faulted);
    REQUIRE(full_state.mode_count==42u);
    REQUIRE(full_cursor.next_bank_word==0x24fb59u && !full_cursor.faulted);
    // Mid-transfer I/O loss is a terminal state, not a pretend rollback.
    CountingPortChip interrupted_chip{};
    interrupted_chip.fail_after_writes=150000u;
    DeviceState interrupted_state{};
    OriginalCreativeRamScratchCycle interrupted_scratch{};
    interrupted_scratch.device=&interrupted_state;
    interrupted_scratch.io=ChipPortBridge<CountingPortChip>::bind(interrupted_chip);
    OriginalCreativeRamBankCursor interrupted_cursor{true,false,0x200000u};
    const auto interrupted=upload_original_creative_sf2_ram_bank(
        first_big.file,interrupted_cursor,
        OriginalCreativeRamChipSink<CountingPortChip>::bind(interrupted_chip),
        interrupted_scratch.hooks());
    REQUIRE(interrupted.status==OriginalSf2UploadStatus::UploadFailed);
    REQUIRE(interrupted_cursor.faulted && interrupted_scratch.faulted);
    REQUIRE(interrupted_cursor.next_bank_word==0x200000u);
    REQUIRE(interrupted_chip.writes>=150000u &&
            interrupted_chip.writes<628142u);
    const auto rejected_recovery=upload_original_creative_sf2_ram_bank(
        first_big.file,interrupted_cursor,
        OriginalCreativeRamChipSink<CountingPortChip>::bind(interrupted_chip),
        interrupted_scratch.hooks());
    REQUIRE(rejected_recovery.status==OriginalSf2UploadStatus::SourceMissing);
    // Hardware failure is NOT transactional. Fail closed permanently.
    cursor=OriginalCreativeRamBankCursor{true,false,0x200000u};
    OriginalRamSink broken_sink{};
    broken_sink.source_words=260921u;broken_sink.fail_at=2u;
    original_status=upload_original_creative_sf2_ram_bank(
        first_big.file,cursor,broken_sink.writer());
    REQUIRE(original_status.status==OriginalSf2UploadStatus::UploadFailed &&
            original_status.physical_sample_writes==1039u &&
            cursor.next_bank_word==0x200000u && cursor.faulted);
    original_status=upload_original_creative_sf2_ram_bank(
        first_big.file,cursor,broken_sink.writer());
    REQUIRE(original_status.status==OriginalSf2UploadStatus::SourceMissing &&
            cursor.next_bank_word==0x200000u);
    // The final 17 guard writes cannot be allowed to run beyond device RAM.
    cursor=OriginalCreativeRamBankCursor{true,false,0x1000000u-1230u};
    OriginalRamSink overflow_sink{};overflow_sink.source_words=1200u;
    original_status=upload_original_creative_sf2_ram_bank(
        authentic.file,cursor,overflow_sink.writer());
    REQUIRE(original_status.status==OriginalSf2UploadStatus::AddressOutOfRange &&
            overflow_sink.call_index==0u);
    std::puts("ORIGINAL_CREATIVE_RAM_SAMPLE_TRANSFER_PASS");
}
