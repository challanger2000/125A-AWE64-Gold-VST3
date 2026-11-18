// P007X: PUBLIC, SYNTHETIC CORE TEST DOUBLE -- never a Creative/Win95 emulator.
// This DLL substitutes for 125A_AWE_Core.dll in an ephemeral Windows CI test
// bundle only. It is NOT shipped, NOT an original EMU8000 audio proof.
// It makes the REAL P007 VST3 run its actual worker, MIDI and process() paths.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <filesystem>

#if defined(_WIN32)
#define P007_MOCK_EXPORT extern "C" __declspec(dllexport)
#else
#error P007 synthetic QA DLL is Windows-only
#endif

namespace {
constexpr std::uint32_t kRingFrames=1u<<16;
float samples[kRingFrames*2u]{};
std::atomic<std::uint64_t> produced{0},consumed{0};
std::atomic<int> ready{0},enabled{0};
std::atomic<unsigned> noteOn{0},noteOff{0},program{0},bank{0},panic{0};
std::atomic<unsigned> bankRejects{0},programRejects{0};
unsigned steps=0;
void produce(unsigned frames) {
    const auto wr=produced.load(std::memory_order_relaxed);
    const auto rd=consumed.load(std::memory_order_acquire);
    const auto available=wr-rd;
    const auto accepted=std::min<std::uint64_t>(
        frames,available<kRingFrames?kRingFrames-available:0u);
    for(unsigned i=0;i<accepted;++i){
        const unsigned offset=unsigned((wr+i)&(kRingFrames-1u))*2u;
        samples[offset]=.25f;
        samples[offset+1]=-0.125f;
    }
    produced.store(wr+accepted,std::memory_order_release);
}
}
P007_MOCK_EXPORT void __cdecl p007_midi_uart_enable(int active) {
    enabled.store(active,std::memory_order_release);
}
P007_MOCK_EXPORT int __cdecl p007_midi_guest_ready() {
    return ready.load(std::memory_order_acquire);
}
P007_MOCK_EXPORT int __cdecl p007_midi_submit(
    std::uint8_t status,std::uint8_t d1,std::uint8_t d2) {
    if(!enabled.load(std::memory_order_acquire)||
       !ready.load(std::memory_order_acquire))return 0;
    // CI fault injection: reject the FIRST actual CC32 bank and program
    // packet so the production VST3 must retry instead of losing the patch.
    if(status==0xb0u && d1==32u && bankRejects.fetch_add(1)==0u)return 0;
    if(status==0xc0u && programRejects.fetch_add(1)==0u)return 0;
    if(status==0x90u&&d2>0u)noteOn.fetch_add(1);
    if(status==0x80u||(status==0x90u&&d2==0u))noteOff.fetch_add(1);
    if(status==0xc0u)program.store(unsigned(d1)+1u);
    if(status==0xb0u&&d1==32u)bank.store(unsigned(d2)+1u);
    if((status&0xf0u)==0xb0u&&d1==123u)panic.fetch_add(1);
    return 1;
}
P007_MOCK_EXPORT std::uint32_t __cdecl p007_audio_pull(float* out,std::uint32_t frames) {
    if(!out)return 0;
    const auto rd=consumed.load(std::memory_order_relaxed);
    const auto wr=produced.load(std::memory_order_acquire);
    const auto available=wr-rd;
    const auto count=std::uint32_t(std::min<std::uint64_t>(available,frames));
    for(unsigned i=0;i<count;++i){
        const unsigned offset=unsigned((rd+i)&(kRingFrames-1u))*2u;
        out[2*i]=samples[offset];out[2*i+1]=samples[offset+1];
    }
    consumed.store(rd+count,std::memory_order_release);
    return count;
}
P007_MOCK_EXPORT std::uint32_t __cdecl p007_audio_trim_backlog(
    std::uint32_t maxBacklog,std::uint32_t retain) {
    const auto rd=consumed.load(std::memory_order_relaxed);
    const auto wr=produced.load(std::memory_order_acquire);
    const auto delta=wr-rd;
    if(delta<=maxBacklog||delta<=retain)return 0;
    consumed.store(wr-retain,std::memory_order_release);
    return std::uint32_t(delta-retain);
}
P007_MOCK_EXPORT int __cdecl p007_core_init_profile(
    const char* vm,const char* rom,const char* name,const char* global) {
    if(!vm||!rom||!name||!global)return 0;
    if(!std::filesystem::is_directory(vm)||
       !std::filesystem::is_directory(rom)||
       !std::filesystem::is_regular_file(global))return 0;
    return 1;
}
P007_MOCK_EXPORT int __cdecl p007_core_guest_boot() {
    produced.store(0);consumed.store(0);steps=0;
    ready.store(0);enabled.store(0);
    noteOn.store(0);noteOff.store(0);program.store(0);bank.store(0);panic.store(0);
    bankRejects.store(0);programRejects.store(0);
    return 1;
}
P007_MOCK_EXPORT int __cdecl p007_core_guest_step() {
    ++steps;
    if(steps>=20u){ready.store(1,std::memory_order_release);enabled.store(1);}
    // 44.1 kHz synthetic constant stereo signal, not Creative content.
    produce((steps%10u)==0u?45u:44u);
    return 1;
}
P007_MOCK_EXPORT int __cdecl p007_core_guest_boot_assist(std::uint32_t) {
    return 0;
}
P007_MOCK_EXPORT void __cdecl p007_core_guest_close() {
    enabled.store(0);ready.store(0);
}
P007_MOCK_EXPORT unsigned __cdecl p007_mock_noteon_count() {return noteOn.load();}
P007_MOCK_EXPORT unsigned __cdecl p007_mock_noteoff_count(){return noteOff.load();}
P007_MOCK_EXPORT unsigned __cdecl p007_mock_program_plus_one(){return program.load();}
P007_MOCK_EXPORT unsigned __cdecl p007_mock_bank_plus_one(){return bank.load();}
P007_MOCK_EXPORT unsigned __cdecl p007_mock_panic_count(){return panic.load();}
P007_MOCK_EXPORT unsigned __cdecl p007_mock_rejected_control_packets(){
    return unsigned(bankRejects.load()+programRejects.load());
}
