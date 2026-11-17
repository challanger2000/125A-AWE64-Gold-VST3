// Source-faithful ORIGINAL 86Box AGI cross-block invariants. No private media.
#include "../research/p109_original_p54c_agi_block_carry.hpp"
#include <cstdint>
#include <iostream>
using namespace awe64::research_p109;
#define REQUIRE(condition) do { if(!(condition)) { \
 std::cerr << "P109_ORIGINAL_AGI_SOURCE_NO_GO line=" << __LINE__ << " guard=" << #condition << "\\n";return 1; } } while(false)

static_assert(kImplicitEsp == 256u);
static_assert(kEsp == 16u);
static_assert(agi_hazard(1u << 0u, 1u << 0u));
static_assert(!agi_hazard(1u << 0u, 1u << 1u));
static_assert(!agi_hazard(kImplicitEsp|kEsp, kImplicitEsp));
static_assert(agi_hazard(kEsp, kImplicitEsp));
static_assert(agi_hazard(kEsp, kEsp));
static_assert(original_p54c_prefix_decode_cost(0x0f, 0x80)==0);
static_assert(original_p54c_prefix_decode_cost(0x0f, 0x8f)==0);
static_assert(original_p54c_prefix_decode_cost(0x0f, 0x90)==1);
static_assert(original_p54c_prefix_decode_cost(0x66, 0x90)==1);
static_assert(original_p54c_prefix_decode_cost(0x66, 0x90, true)==2);
static_assert(original_p54c_prefix_decode_cost(0xd9, 0x00)==0);
static_assert(!PentiumOriginalAgiEvidenceOnly::autonomous_original_guest_scheduler_ready());

int main() {
    constexpr std::uint32_t eax=1u<<0, ebx=1u<<3, edx=1u<<2;
    // Critical ORIGINAL source behavior: block_start does NOT reset
    // last translated instruction's dest mask.
    PentiumOriginalAgiEvidenceOnly s;
    REQUIRE(!s.check_original_address_agi(eax));
    s.on_nonpaired_opcode_write(eax);
    s.on_original_block_start();
    REQUIRE(s.carried_modified_mask()==eax);
    REQUIRE(s.check_original_address_agi(eax));
    REQUIRE(!s.check_original_address_agi(ebx));
    // Source keeps mask through block_end, even after flushing U pipe.
    s.on_u_pipe_deferred();
    REQUIRE(s.u_pipe_pending());
    s.on_original_block_end();
    REQUIRE(!s.u_pipe_pending());
    REQUIRE(s.carried_modified_mask()==eax);
    s.on_original_block_start();
    REQUIRE(s.check_original_address_agi(eax));
    // A new explicitly written dest mask replaces previous mask.
    s.on_nonpaired_opcode_write(ebx);
    REQUIRE(!s.check_original_address_agi(eax));
    REQUIRE(s.check_original_address_agi(ebx));
    // 86Box recorded U/V pair registers are unioned, not overwritten.
    s.on_original_paired_opcode_writes(eax,edx);
    s.on_original_block_start();
    REQUIRE(s.carried_modified_mask()==(eax|edx));
    REQUIRE(s.check_original_address_agi(edx));
    REQUIRE(s.check_original_address_agi(eax));
    REQUIRE(!s.check_original_address_agi(ebx));
    // Implicit ESP has explicit source-level exemption.
    s.on_nonpaired_opcode_write(kEsp|kImplicitEsp);
    s.on_original_block_start();
    REQUIRE(!s.check_original_address_agi(kImplicitEsp));
    s.on_nonpaired_opcode_write(kEsp);
    s.on_original_block_start();
    REQUIRE(s.check_original_address_agi(kImplicitEsp));
    // Source-block first-op AGI is not proof of guest hardware TSC timing.
    s.on_nonpaired_opcode_write(0);
    s.on_original_block_start();
    REQUIRE(!s.check_original_address_agi(eax));
    REQUIRE(s.decode_delay()==0 && s.decode_delay_offset()==0);
    // All 0F 8x near conditional jumps use free prefix under P54C source.
    for (unsigned i=0x80;i<=0x8f;++i) {
        REQUIRE(original_p54c_prefix_decode_cost(0x0f,static_cast<std::uint8_t>(i))==0);
    }
    for (unsigned i=0;i<25000;++i) {
        s.on_nonpaired_opcode_write((i&1)?eax:ebx);
        s.on_original_block_start();
        REQUIRE(s.check_original_address_agi((i&1)?eax:ebx));
    }
    std::cout << "P109_P54C_ORIGINAL_AGI_CROSS_BLOCK_EVIDENCE_PASS "
              << "exact_source_86box=v6.0-AWE32-0.93 "
              << "masked_deps=PASS esp_exceptions=PASS "
              << "jit_block_start_mask_carried=PASS "
              << "short_and_near_jcc_prefix=PASS "
              << "actual_guest_x86_tsc=NOT_MEASURED "
              << "vst3_scheduler_ready=FALSE\n";
    return 0;
}
