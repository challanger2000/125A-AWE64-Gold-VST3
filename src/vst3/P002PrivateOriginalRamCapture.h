#pragma once
// P002 EXPLICIT PRIVATE RESEARCH GATE, never normal/production bank mode.
// Replays the exact 1996 Creative Win95 captured filtered RAM port stream
// on the ORIGINAL unchanged calibrated EMU8000 during VST3 setActive().
// The bank, original final sample image and original port stream are
// separately SHA256 pinned to the owner's authenticated 2026-11-13
// original Windows95/86Box recording; no copyrighted data embedded.
// This preloads authentic physical RAM but DOES NOT invent driver
// NoteOn voice/register programming or certify VST3 soundbank playback.
// NEVER call it on the process() audio thread.
#include "Chip.h"
#include <filesystem>
#include <string>
namespace awe64::vst3 {
[[nodiscard]] bool preloadOriginalCreativeCapturedRamSource(
    awe64::Chip& unchangedOriginalEmu8000,
    const std::filesystem::path& privateOwnerSf2,
    const std::filesystem::path& originalFilteredCapturedRamPorts,
    const std::filesystem::path& observedOriginalFinalBankWordsPcm16,
    std::string& error);
} // namespace awe64::vst3
