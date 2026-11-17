#pragma once
// P005 research: ORIGINAL source-derived Pentium P54C integer lookup bridge.
//
// The timing/dependency words are NOT supplied by callers or guessed.
// They come from the 40,960-original-selection-differential P116 source
// tables generated at build time from SHA-pinned public 86Box C sources.
// A SOURCE-AUTHENTICATED, branch-correct x86 decoder must separately feed
// last_prefix/opcode/fetchdat/op_32/prefix bytes and translation generation.
// This does NOT execute x86 or imply native Win95 NoteOn autonomy.
//
// COPYRIGHT BOUNDARY: P116 table output is generated into the ephemeral
// build directory only. No GPL 86Box derived arrays, proprietary VXD,
// ROM, SF2 or original guest traces are committed to this product branch.
#include "P005OriginalPentiumCompiledBlockClock.hpp"
#include "P116OriginalP54cOpcodeLookup.hpp"
#include <cstdint>
#include <cstddef>
#include <span>

namespace awe64::creative {
struct P005OriginalDecoderOpcodeInput final {
    std::uint8_t last_prefix=0u; // 0x00 or supported two-byte 0x0f
    std::uint8_t opcode=0u;
    std::uint32_t fetchdat=0u; // original 86Box ModR/M + SIB bits
    std::uint16_t original_op_32=0u; // 86Box 0x100/0x200 bits
    std::span<const std::uint8_t> original_prefix_bytes{};
};
class P005OriginalP54cOpcodeSourceFeed final {
public:
    [[nodiscard]] static bool appendSourceDecodedInteger(
        P005OriginalPentiumCompiledBlockClock& blocks,
        const P005OriginalDecoderOpcodeInput& event) noexcept {
        if((event.original_op_32 & static_cast<std::uint16_t>(~0x300u))!=0u ||
           event.original_prefix_bytes.size()>8u ||
           (event.last_prefix!=0u && event.last_prefix!=0x0fu))
            return blocks.rejectUnverifiedSourceOpcode();
        bool has0f=false;
        for(auto b:event.original_prefix_bytes) {
            if(b==0x0fu) {
                if(has0f)return blocks.rejectUnverifiedSourceOpcode();
                has0f=true;
            }
        }
        if(has0f!=(event.last_prefix==0x0fu))
            return blocks.rejectUnverifiedSourceOpcode();
        const auto source=awe64::research_p116::originalP54cLookup(
            event.last_prefix,event.opcode,event.fetchdat);
        if(!source || !source->integer_subset)
            return blocks.rejectUnverifiedSourceOpcode();
        P005OriginalP54cDecodedIntegerOp original{};
        original.timing_word=source->timing_word;
        original.dependency_word=source->dependency_word;
        original.fetchdat=event.fetchdat;
        original.op_32=event.original_op_32;
        original.opcode=event.opcode;
        original.prefixes=event.original_prefix_bytes;
        return blocks.translationOpcode(original);
    }
};
} // namespace awe64::creative
