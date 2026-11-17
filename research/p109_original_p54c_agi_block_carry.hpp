#pragma once
// P109: source-faithful, intentionally bounded ORIGINAL 86Box P54C
// translation STATE evidence, never native VST3 scheduler/DSP.
//
// Reference: turican0/86Box-AWE32 v6.0-AWE32-0.93
//   src/cpu/codegen_timing_pentium.c:
//     1000-1010: check_agi(), incl. implicit ESP exception
//     958-961: block_start() resets u_pipe/decode; NOT regmask_modified
//     1219-1224,1259-1264,1270-1275,1301-1306: write-mask update
//     1309-1318: block_end flushes pending U; does NOT update regmask_modified
//   src/cpu/codegen_timing_common.h: REGMASK_IMPL_ESP == (1 << 8)
//   src/cpu/codegen_timing_pentium.c: 971-997: 0F near-Jcc prefix "free"
// This models ONLY those verifiable effects; no decoded real x86 path,
// AGI source reconstruction, 86Box translation order, block TSC,
// chip I/O, Guest IRQ/Windows events or bit exact native PCM.
#include <cstdint>

namespace awe64::research_p109 {

inline constexpr std::uint32_t kImplicitEsp = 1u << 8u;
inline constexpr std::uint32_t kEsp = 1u << 4u;

// Original 86Box P54C AGI check, once x86 ModR/M source dependencies are
// *independently* decoded from original instruction and original flags.
[[nodiscard]] constexpr bool agi_hazard(std::uint32_t previous_modified,
                                        std::uint32_t next_address_regs) noexcept {
    std::uint32_t address = next_address_regs;
    if ((address & kImplicitEsp) != 0u &&
        (previous_modified & kEsp) != 0u &&
        (previous_modified & kImplicitEsp) == 0u)
        address |= kEsp;
    return ((previous_modified & address) & ~kImplicitEsp) != 0u;
}

// Explicit original block-start persistence behavior. This state represents
// *translator compilation order*, not a separately observed P54C hardware
// register hazard across an executed branch.
class PentiumOriginalAgiEvidenceOnly final {
    std::uint32_t modified_ = 0u;
    bool u_pipe_pending_ = false;
    int decode_delay_ = 0;
    int decode_delay_offset_ = 0;

public:
    void on_original_block_start() noexcept {
        u_pipe_pending_ = false;
        decode_delay_ = 0;
        decode_delay_offset_ = 0;
        // Deliberately do NOT zero modified_: original 86Box does not.
    }
    void on_nonpaired_opcode_write(std::uint32_t destination_mask) noexcept {
        modified_ = destination_mask;
    }
    void on_original_paired_opcode_writes(std::uint32_t u_write_mask,
                                          std::uint32_t v_write_mask) noexcept {
        modified_ = u_write_mask | v_write_mask;
    }
    void on_u_pipe_deferred() noexcept { u_pipe_pending_ = true; }
    void on_original_block_end() noexcept {
        // P54C original final pending U cost is added to block cycles,
        // but regmask_modified is NOT written by block_end().
        u_pipe_pending_ = false;
    }
    [[nodiscard]] bool check_original_address_agi(std::uint32_t read_mask) const noexcept {
        return agi_hazard(modified_, read_mask);
    }
    [[nodiscard]] std::uint32_t carried_modified_mask() const noexcept {
        return modified_;
    }
    [[nodiscard]] bool u_pipe_pending() const noexcept { return u_pipe_pending_; }
    [[nodiscard]] int decode_delay() const noexcept { return decode_delay_; }
    [[nodiscard]] int decode_delay_offset() const noexcept { return decode_delay_offset_; }
    [[nodiscard]] static constexpr bool autonomous_original_guest_scheduler_ready() noexcept {
        return false;
    }
};

[[nodiscard]] constexpr std::uint8_t original_p54c_prefix_decode_cost(
    const std::uint8_t prefix, const std::uint8_t next_fetchdat_low,
    const bool mmx = false) noexcept {
    if ((prefix & 0xf8u) == 0xd8u) return 0u; // FPU opcode, not prefix cycle
    if (mmx && prefix == 0x0fu) return 0u;
    if (mmx && (prefix == 0x66u || prefix == 0x67u)) return 2u;
    if (prefix == 0x0fu && (next_fetchdat_low & 0xf0u) == 0x80u)
        return 0u; // original Pentium near Jcc 0F 80-8F decoding
    return 1u; // other prefixes, shadowable by prior execution
}

} // namespace awe64::research_p109
