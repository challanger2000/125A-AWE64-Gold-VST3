// Busy-voice regression for the source-reconstructed PCOD+0x26AF RAM
// scratch allocator. Synthetic chip state only: this is NOT a claimed
// Win95 bank-2 differential or a replay of original register reads.
#include "OriginalCreativeRamScratchVoices.hpp"
#include "OriginalCreativeChipBridge.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"BUSY_SCRATCH_FAIL:%d %s\n",__LINE__,#x);std::exit(1);}}while(false)
namespace {
struct RegisterChip {
    bool initialized=true;
    std::uint8_t selector=0;
    std::array<std::uint32_t,32u> vtft{},ccca{};
    std::array<std::uint16_t,32u> dcysusv{};
    std::uint64_t writes=0u,reads=0u,vtft_reads=0u,ccca_reads=0u;
    [[nodiscard]] bool ready() const noexcept {return initialized;}
    void writePort(std::uint16_t port,std::uint16_t value) noexcept {
        ++writes;
        if(port==0xe22u){selector=static_cast<std::uint8_t>(value);return;}
        const auto voice=static_cast<std::uint8_t>(selector&31u);
        const auto reg=static_cast<std::uint8_t>(selector&0xe0u);
        if(reg==0x60u && (port==0x620u || port==0x622u)) {
            const auto shift=(port==0x620u)?0u:16u;
            vtft[voice]=(vtft[voice]&~(0xffffu<<shift)) |
                (static_cast<std::uint32_t>(value)<<shift);
        } else if(reg==0xa0u && port==0xa20u) {
            dcysusv[voice]=value;
        }
    }
    [[nodiscard]] std::uint16_t readPort(std::uint16_t port) noexcept {
        ++reads;
        const auto voice=static_cast<std::uint8_t>(selector&31u);
        const auto reg=static_cast<std::uint8_t>(selector&0xe0u);
        if(reg==0x60u && (port==0x620u || port==0x622u)) {
            ++vtft_reads;
            return static_cast<std::uint16_t>(vtft[voice] >> ((port==0x620u)?0u:16u));
        }
        if(reg==0x00u && (port==0xa20u || port==0xa22u)) {
            ++ccca_reads;
            return static_cast<std::uint16_t>(ccca[voice] >> ((port==0xa20u)?0u:16u));
        }
        if(reg==0xa0u && port==0xa20u)return dcysusv[voice];
        return 0u;
    }
};
}
int main(){
    RegisterChip chip{};
    DeviceState state{};
    // Model a loaded bank with retained sample-end markers and nonzero
    // hardware volume envelopes. All values are synthetic test inputs.
    for(std::size_t v=0;v<32u;++v){
        chip.vtft[v]=(0x1000u+static_cast<std::uint32_t>(v))<<16u;
        chip.ccca[v]=0x00200000u+static_cast<std::uint32_t>(v);
        chip.dcysusv[v]=0x8000u;
        state.voices[v].sample_end=0x00700000u;
    }
    OriginalCreativeRamScratchCycle scratch{};
    scratch.io=ChipPortBridge<RegisterChip>::bind(chip);
    scratch.device=&state;
    for(unsigned i=0;i<65u;++i){
        CHECK(scratch.before_block());
        CHECK(scratch.active && !scratch.faulted);
        for(auto const id:scratch.selected)CHECK(id<32u);
        CHECK(scratch.after_block());
        CHECK(!scratch.active && !scratch.faulted);
    }
    CHECK(scratch.cycles_completed==65u);
    CHECK(scratch.selected_voices==650u);
    CHECK(state.mode_count==42u);
    // A clean first-bank fake chip returns only zero and never tests
    // CCCA reads. A busy voice state must actually invoke the original
    // live read path (both halves, before choosing the cheapest voice).
    CHECK(chip.ccca_reads>0u && chip.vtft_reads>chip.ccca_reads);
    CHECK(chip.reads>65u*20u && chip.writes>65u*410u);
    CHECK(state.voices[30].sample_end==0x00700000u);
    CHECK(chip.ccca[30]==0x0020001eu);
    std::printf("SOURCE_RAM_BUSY_VOICES_PASS cycles=%u chosen=%u reads=%llu vtft=%llu ccca=%llu writes=%llu\n",
        scratch.cycles_completed,scratch.selected_voices,
        static_cast<unsigned long long>(chip.reads),
        static_cast<unsigned long long>(chip.vtft_reads),
        static_cast<unsigned long long>(chip.ccca_reads),
        static_cast<unsigned long long>(chip.writes));
}
