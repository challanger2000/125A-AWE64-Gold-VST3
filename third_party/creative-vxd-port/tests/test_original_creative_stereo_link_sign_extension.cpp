#include "OriginalCreativeStereoWorklist.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>

using awe64::original_vxd::Descriptor;
using awe64::original_vxd::WorkStatus;
using awe64::original_vxd::original_stereo_worklist;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "Creative MOVSX/MOVZX source mismatch at line %d: %s\\n", \
                     __LINE__, #condition); \
        std::exit(1); \
    } \
} while (false)

namespace {
void w16(Descriptor& d, const unsigned offset, std::uint16_t value) {
    d[offset] = static_cast<std::uint8_t>(value & 0xffu);
    d[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
}

[[nodiscard]] bool original_pairs(std::uint16_t id, std::uint16_t link) {
    std::array<Descriptor,2> descriptors{};
    // First row: original right-sample (0x8c bit1), link at 0x8e.
    w16(descriptors[0],0x8c,2u);
    w16(descriptors[0],0x8e,link);
    // Second row: possible left partner; sampleID at 0x6a.
    w16(descriptors[1],0x8c,4u);
    w16(descriptors[1],0x6a,id);
    const auto work=original_stereo_worklist(
        std::span<const Descriptor>{descriptors});
    CHECK(work.status==WorkStatus::Ok);
    CHECK(work.count==2u);
    if (work.stereo_pairs != 0u) {
        CHECK(work.stereo_pairs==1u);
        CHECK(work.items[0].source_index==1u);
        CHECK(work.items[0].process_type==4u);
        CHECK(work.items[1].source_index==0u);
        CHECK(work.items[1].process_type==2u);
        return true;
    }
    CHECK(work.items[0].process_type==1u);
    CHECK(work.items[1].process_type==1u);
    return false;
}
} // namespace

int main() {
    // All boundary values are derived from original x86 mixed widths:
    // MOVSX 16->32(candidate ID); MOVZX 16->32(link); CMP DWORD.
    CHECK(original_pairs(0x0000u,0x0000u));
    CHECK(original_pairs(0x0001u,0x0001u));
    CHECK(original_pairs(0x7ffeu,0x7ffeu));
    CHECK(original_pairs(0x7fffu,0x7fffu));

    CHECK(!original_pairs(0x8000u,0x8000u));
    CHECK(!original_pairs(0x8001u,0x8001u));
    CHECK(!original_pairs(0xfffeu,0xfffeu));
    CHECK(!original_pairs(0xffffu,0xffffu));

    CHECK(!original_pairs(0x7fffu,0x8000u));
    CHECK(!original_pairs(0x8000u,0x7fffu));
    CHECK(!original_pairs(0x0001u,0xffffu));
    CHECK(!original_pairs(0xffffu,0x0001u));
    CHECK(!original_pairs(0x0001u,0x0002u));

    // Exhaustively verify the mathematical effect of original MOVSX/MOVZX
    // for every possible 16-bit value; no driver binary/raw byte required.
    unsigned matched=0u;
    for (std::uint32_t v=0;v<=0xffffu;++v) {
        const auto sign_extended=static_cast<std::int32_t>(
            static_cast<std::int16_t>(static_cast<std::uint16_t>(v)));
        const auto zero_extended=static_cast<std::int32_t>(v);
        const bool expected=(sign_extended==zero_extended);
        CHECK(expected==(v<=0x7fffu));
        ++matched;
    }
    CHECK(matched==65536u);
    std::puts("CREATIVE_LCOD_545F_MOVSX_5465_MOVZX_546C_CMP_PASS");
    return 0;
}
