// P003 original-SBAWE.VXD stereo gate RESEARCH ORACLE (C++23 only).
// Narrow, independent source-matched regression, NOT a driver port or DSP timer.
// LCOD +3FFD/+4001/+4023/+4027/+4029/+4033/+4037/+403A/+403C/+403E/+4042
//      +4048/+404F/+4054. Verified original VXD SHA256:
// d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a
#include <array>
#include <cstddef>
#include <cstdint>

namespace p003_original_voice_gate {

enum class Action : std::uint8_t { read_vtft, skip_byte_0d, skip_byte_2d, skip_marker };
struct Input {
    std::uint32_t mode_esi_plus_18;
    std::uint16_t count_esi_plus_06;
    std::uint32_t edi;
    std::uint8_t  byte_at_ebx_plus_0d;
    std::uint8_t  byte_at_ebx_plus_2d;
    std::uint16_t marker_ebx_minus_0a; // NOT current score
};
constexpr Action evaluate(Input x) noexcept {
    if (x.mode_esi_plus_18 <= 2) {
        if (x.byte_at_ebx_plus_0d != 0)
            return Action::skip_byte_0d;
        // Original LEA ECX,[EDI+1] wraps in 32-bit arithmetic.
        if (static_cast<std::uint32_t>(x.count_esi_plus_06) >
            (x.edi + std::uint32_t{1}) && x.byte_at_ebx_plus_2d != 0)
            return Action::skip_byte_2d;
    }
    return x.marker_ebx_minus_0a >= 0x2000 ? Action::skip_marker : Action::read_vtft;
}

constexpr bool original_branch_regression() {
    constexpr Input base{2, 32, 26, 0, 0, 0x0224};
    if (evaluate(base) != Action::read_vtft) return false;
    auto v=base;
    v.byte_at_ebx_plus_0d=1;
    if (evaluate(v) != Action::skip_byte_0d) return false;
    v=base; v.byte_at_ebx_plus_2d=1;
    if (evaluate(v) != Action::skip_byte_2d) return false;
    // Count <= EDI+1 bypasses the second gate.
    v.count_esi_plus_06=27;
    if (evaluate(v) != Action::read_vtft) return false;
    // In mode >2 both neighbor-flag checks are bypassed.
    v=base;v.mode_esi_plus_18=3;
    v.byte_at_ebx_plus_0d=1;v.byte_at_ebx_plus_2d=1;
    if (evaluate(v) != Action::read_vtft) return false;
    v.marker_ebx_minus_0a=0x2000;
    if (evaluate(v) != Action::skip_marker) return false;
    v=base;v.marker_ebx_minus_0a=0x1fff;
    if (evaluate(v) != Action::read_vtft) return false;
    v.marker_ebx_minus_0a=0x2000;
    if (evaluate(v) != Action::skip_marker) return false;
    // The original loop does EBX-=0x20. Existing V5 lookahead
    // reads -19 and +13 on current row for NEXT row +0x0D/+0x2D.
    std::array<std::uint8_t,128> rows{};
    constexpr std::size_t current=64,next=current-32;
    rows[current-19]=0x37;
    rows[current+13]=0x69;
    rows[current+45]=0x95; // deliberately distinct wrong current +2D
    if (rows[current-19]!=rows[next+0x0d]) return false;
    if (rows[current+13]!=rows[next+0x2d]) return false;
    if (rows[next+0x2d]==rows[current+0x2d]) return false;
    return true;
}
static_assert(original_branch_regression());
} // namespace
int main() {
    return p003_original_voice_gate::original_branch_regression()?0:1;
}
