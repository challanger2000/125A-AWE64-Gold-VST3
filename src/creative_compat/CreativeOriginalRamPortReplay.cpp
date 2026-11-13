#include "CreativeOriginalRamPortReplay.h"
#include <cstddef>
#include <cstdint>
#include <optional>
namespace awe64::creative {
namespace {
constexpr std::uint16_t kPointerSelect=0x36u; // (1u<<5) | 22u: SMALW
constexpr std::uint16_t kDataSelect=0x3au; // (1u<<5) | 26u: SMLD
constexpr std::uint16_t kVoiceSelectorPort=0xe22u;
constexpr std::uint16_t kDataLowPort=0xa20u;
constexpr std::uint16_t kDataHighPort=0xa22u;
// Original source and the 86Box port interface use absolute EMU8000
// RAM WORD addresses (0x200000..0xffffdf), not byte offsets.
constexpr std::uint32_t kFirstRamWord=0x200000u;
constexpr std::uint32_t kExclusiveFmReservedWord=0xffffe0u;
constexpr std::size_t kMaxCapturedPortWrites=0xE00000u*2u+1024u;
struct Analysis {
    OriginalRamUploadReplayResult result{};
    std::uint32_t current=0u;
    std::uint16_t low=0u,high=0u,selected=0xffffu;
    bool hasLow=false,hasHigh=false,hasAddress=false;
};
[[nodiscard]] bool validAddress(const std::uint32_t p) noexcept {
    return p>=kFirstRamWord && p<kExclusiveFmReservedWord;
}
[[nodiscard]] std::optional<OriginalRamUploadReplayResult> preflight(
    const std::span<const OriginalRamUploadPortWrite> trace) noexcept {
    if(trace.empty() || trace.size()>kMaxCapturedPortWrites)
        return std::nullopt;
    Analysis a{};
    a.result.exact_port_writes=trace.size();
    a.result.first_chip_frame=trace.front().original_chip_frame;
    a.result.last_chip_frame=trace.back().original_chip_frame;
    for(std::size_t i=0u;i<trace.size();++i) {
        const auto& e=trace[i];
        if(i>0u && e.original_chip_frame<
                    trace[i-1u].original_chip_frame)
            return std::nullopt;
        if(e.port==kVoiceSelectorPort) {
            // This is a *filtered RAM upload stream*; unrelated
            // original NOTE/FX writes must stay in the independent
            // whole-trace oracle, not get reinterpreted as SMLD.
            if(e.value!=kPointerSelect && e.value!=kDataSelect)
                return std::nullopt;
            a.selected=e.value;
            if(e.value==kPointerSelect) {
                a.hasLow=false;
                a.hasHigh=false;
                a.hasAddress=false;
            }
        }else if(a.selected==kPointerSelect) {
            if(e.port==kDataLowPort) {
                if(a.hasLow)return std::nullopt;
                a.low=e.value;a.hasLow=true;
            }else if(e.port==kDataHighPort) {
                if(a.hasHigh)return std::nullopt;
                a.high=e.value;a.hasHigh=true;
            }else return std::nullopt;
            if(a.hasLow && a.hasHigh) {
                // Do NOT invent a driver RAM placement/alignment:
                // faithfully use the complete 32-bit original chip
                // address written to SMALW by the captured Win95 VXD.
                a.current=static_cast<std::uint32_t>(a.low)|
                    (static_cast<std::uint32_t>(a.high)<<16u);
                if(!validAddress(a.current))return std::nullopt;
                a.hasAddress=true;
            }
        }else if(a.selected==kDataSelect) {
            if(e.port!=kDataLowPort || !a.hasAddress ||
               !validAddress(a.current))return std::nullopt;
            if(a.result.original_sample_words==0u)
                a.result.first_word_address=a.current;
            a.result.last_word_address=a.current;
            ++a.result.original_sample_words;
            ++a.current; // original SMALW/SMLD hardware auto-increment
        }else return std::nullopt;
    }
    if(a.result.original_sample_words==0u ||
       // A dangling/partial final SMALW is not an authentic finished
       // original upload transaction.
       (a.selected==kPointerSelect && !a.hasAddress))
        return std::nullopt;
    return a.result;
}
} // namespace
std::optional<OriginalRamUploadReplayResult>
candidateReplayOriginalCreativeCapturedRamUpload(
    awe64::Chip& chip,
    const std::span<const OriginalRamUploadPortWrite> trace,
    const bool original_trace_provenance_verified) noexcept {
    if(!original_trace_provenance_verified || !chip.ready())
        return std::nullopt;
    const auto plan=preflight(trace);
    if(!plan)return std::nullopt;
    // No memory allocation, no guessed buffering. All ports/values
    // are owned by the independently recorded guest's ordered trace.
    // Original 86Box/turican0 emu8k_outw owns the actual DRAM writes.
    for(const auto& event:trace)
        chip.writePort(event.port,event.value);
    return *plan;
}
} // namespace awe64::creative
