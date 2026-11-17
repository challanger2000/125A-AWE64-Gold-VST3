#pragma once
// Unit-test-only stand-in for the exact 3-field, POD ChipPortWrite
// at src/chip_adapter/Chip.h. The actual installed Windows VST3
// continues including the ORIGINAL Chip.h and original 86Box headers.
// This allows testing only the source-correct autonomous CLOCK MATH in
// a small Linux g++ process. It is not a substitute for VST3/EMU tests.
#include <cstdint>
namespace awe64 {
struct ChipPortWrite {
    std::uint64_t frame=0;
    std::uint16_t port=0;
    std::uint16_t value=0;
};
class Chip;
}
