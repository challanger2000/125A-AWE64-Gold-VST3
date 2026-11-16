#pragma once
// P105: bounded, source-derived *pairing-only* clock component.
//
// Source: turican0/86Box-AWE32 v6.0-AWE32-0.93
//   src/cpu/codegen_timing_pentium.c: COUNT and pair_timings;
//   src/cpu/cpu.c: CPU_PENTIUM -> codegen_timing_pentium.
// P003 owner Win95 VM: pentium_p54c @133333333Hz, cpu_use_dynarec=1.
//
// Strict limitation: no prefetch, AGI, prefix decode, FPU, block translation,
// branch-path instruction selection, hardware I/O, timer/interrupt, or
// fractional 44.1kHz phase is modeled here. NOT a native VST3 scheduler.
// Never feed the result into product register write timestamps.
#include <cstdint>
#include <limits>

namespace awe64::research_p105 {

enum class PentiumOpClass : std::uint8_t {
    Reg = 0, ReadMem = 1, ReadModifyWrite = 2, Branch = 3
};
enum class PentiumPairClass : std::uint8_t {
    NonPairable = 0, UOnly = 1, VOnly = 2, Either = 3
};

// These are *known* source-dependency masks only; unverified dependencies
// must be marked unsupported, not silently treated as independent.
struct DecodedOp {
    PentiumOpClass cost_class;
    PentiumPairClass allowed_pipe;
    std::uint32_t registers_read = 0;
    std::uint32_t registers_written = 0;
    bool has_address_displacement = false;
    bool agi_stall = false;
    bool supported_dependency_decode = false;
};

// Exactly the integer non-FPU COUNT() part of the P54C model.
// MMX instructions and all other 86Box CPU model types are excluded.
[[nodiscard]] constexpr std::uint8_t unpaired_p54c_cycles(
    PentiumOpClass c) noexcept {
    switch (c) {
        case PentiumOpClass::Reg: return 1;
        case PentiumOpClass::ReadMem: return 2;
        case PentiumOpClass::ReadModifyWrite: return 3;
        case PentiumOpClass::Branch: return 2;
    }
    return 0;
}
[[nodiscard]] constexpr std::uint8_t paired_p54c_cycles(
    PentiumOpClass u, PentiumOpClass v) noexcept {
    constexpr std::uint8_t table[4][4] = {
        {1,2,3,2},  // U register op followed by V
        {2,2,3,3},  // U memory read
        {3,4,5,4},  // U read/modify/write
        {0,0,0,0}   // branch must never occupy U
    };
    return table[static_cast<unsigned>(u)][static_cast<unsigned>(v)];
}

// Deliberately refuses claims of a complete x86 timing model.
// A 86Box-realistic full scheduler needs its actual instruction stream,
// dependencies, AGI stalls and guest/host sample-phase mapping.
class PentiumP54cPairingOnly {
    DecodedOp pending_{};
    bool pending_valid_ = false;
    bool valid_ = true;
    std::uint64_t cycles_ = 0;
    std::uint64_t emitted_instructions_ = 0;
    std::uint64_t paired_instructions_ = 0;

    static constexpr bool can_use_u(PentiumPairClass p) noexcept {
        return p==PentiumPairClass::UOnly || p==PentiumPairClass::Either;
    }
    static constexpr bool can_use_v(PentiumPairClass p) noexcept {
        return p==PentiumPairClass::VOnly || p==PentiumPairClass::Either;
    }
    void flush() noexcept {
        if (!pending_valid_) return;
        cycles_ += unpaired_p54c_cycles(pending_.cost_class);
        pending_valid_ = false;
    }

public:
    // Does not allocate, throw, or perform guest I/O.
    [[nodiscard]] bool add(DecodedOp op) noexcept {
        if (!valid_) return false;
        if (!op.supported_dependency_decode || op.agi_stall ||
            static_cast<unsigned>(op.cost_class) > 3u ||
            static_cast<unsigned>(op.allowed_pipe) > 3u ||
            emitted_instructions_==std::numeric_limits<std::uint64_t>::max()) {
            valid_=false;
            return false;
        }
        ++emitted_instructions_;
        if (pending_valid_) {
            // From 86Box: the V-pipe instruction must have no source GPR
            // dependency on the U-pipe instruction's written GPRs.
            const bool independent=
                (pending_.registers_written & op.registers_read)==0u;
            const auto paircost=paired_p54c_cycles(pending_.cost_class,
                                                   op.cost_class);
            if (can_use_v(op.allowed_pipe) && independent &&
                !op.has_address_displacement && paircost!=0u) {
                cycles_ += paircost;
                pending_valid_ = false;
                paired_instructions_ += 2u;
                return true;
            }
            flush();
        }
        if (can_use_u(op.allowed_pipe) && !op.has_address_displacement)
            pending_ = op, pending_valid_=true;
        else
            cycles_ += unpaired_p54c_cycles(op.cost_class);
        return true;
    }
    void end_x86_block() noexcept { flush(); }
    [[nodiscard]] bool valid() const noexcept { return valid_; }
    [[nodiscard]] std::uint64_t partial_pairing_cycles() const noexcept {
        return cycles_ + (pending_valid_ ?
            unpaired_p54c_cycles(pending_.cost_class) : 0u);
    }
    [[nodiscard]] std::uint64_t observed_instruction_count() const noexcept {
        return emitted_instructions_;
    }
    [[nodiscard]] std::uint64_t paired_instruction_count() const noexcept {
        return paired_instructions_;
    }
    [[nodiscard]] static constexpr bool autonomous_scheduler_ready() noexcept {
        return false;
    }
};
} // namespace awe64::research_p105
