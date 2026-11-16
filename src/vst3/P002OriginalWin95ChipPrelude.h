#pragma once
// Research-only original Win95 Creative EMU8000 pre-NoteOn state/time replay.
// Source is a private, SHA256-attested ORIGINAL 86Box/Win95 trace; no source
// port values, ROM or sample data live in this repository or release.
#include "Chip.h"
#include <cstdint>
#include <filesystem>
#include <string>

namespace awe64::vst3 {
struct OriginalChipPreludeResult {
    std::uint32_t physical_writes=0;
    std::uint32_t physical_reads=0;
    std::uint32_t source_init_writes=0;
    std::uint32_t source_hardware_config_writes=0;
    std::uint32_t read_disagreements=0;
    std::uint32_t first_read_disagreement_frame=0;
    std::uint16_t first_read_disagreement_port=0;
    std::uint16_t first_read_disagreement_selector=0;
    std::uint16_t first_read_disagreement_source=0;
    std::uint16_t first_read_disagreement_native=0;
    std::uint64_t source_chip_clock_at_activation=0;
};
// Runs ONLY on the VST3 activation thread before audio and before arming
// native NoteOn write/PCM evidence. Replays the ORIGINAL boot/prelude port
// traffic to establish original EMU8000 state, NEVER the original C4/D4
// note port writes. The note compiler remains independent from this trace.
bool replayPrivateOriginalWin95ChipPrelude(
    awe64::Chip& chip,
    const std::filesystem::path& ownerOriginalFullTrace,
    const std::filesystem::path& alreadyVerifiedFinalRamPcm16,
    std::uint32_t nativeFirstC4Frame,
    OriginalChipPreludeResult& report,
    std::string& error);
} // namespace awe64::vst3
