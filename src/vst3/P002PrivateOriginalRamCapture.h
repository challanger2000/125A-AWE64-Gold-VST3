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
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
namespace awe64::vst3 {
[[nodiscard]] bool preloadOriginalCreativeCapturedRamSource(
    awe64::Chip& unchangedOriginalEmu8000,
    const std::filesystem::path& privateOwnerSf2,
    const std::filesystem::path& originalFilteredCapturedRamPorts,
    const std::filesystem::path& observedOriginalFinalBankWordsPcm16,
    std::string& error);

// Only the four authenticated original Win95 source NOTE PORT GROUPS,
// including their measured 0..2 original EMU8000-frame event spacing,
// may run on the native VST3. This is NOT a general RAM-SF2 synthesizer.
// No source bytes are compiled in; every owner-side note capture file
// is independently SHA256 authenticated before real chip activation.
// Files are only loaded on setActive(), never in an audio callback.
class P002PrivateOriginalRamVoices final {
public:
    [[nodiscard]] bool load(
        const std::array<std::filesystem::path,4>& private_source_groups,
        std::string& error);
    void clear() noexcept;
    [[nodiscard]] bool ready() const noexcept {return ready_;}
    [[nodiscard]] bool pending() const noexcept {return pending_group_<2u;}
    [[nodiscard]] bool noteOn(std::uint8_t channel,std::uint8_t key,
                              std::uint8_t velocity) noexcept;
    [[nodiscard]] bool noteOff(awe64::Chip& chip,std::uint8_t channel,
                               std::uint8_t key) noexcept;
    // Before EACH corresponding chip.render(1), apply only source
    // registers at that specific original relative chip sample-frame.
    // Bounded fixed arrays, no heap/locks/I/O; never generate hardware
    // values from speculative ADSR, filter, sample address or allocator.
    void advanceOneChipFrame(awe64::Chip& chip) noexcept;
private:
    struct SourcePort {
        std::uint32_t relative_frame=0u;
        std::uint16_t port=0u,value=0u;
    };
    std::array<SourcePort,160> c4_on_{};
    std::array<SourcePort,16> c4_off_{};
    std::array<SourcePort,160> d4_on_{};
    std::array<SourcePort,16> d4_off_{};
    std::uint16_t c4_on_count_=0u,c4_off_count_=0u;
    std::uint16_t d4_on_count_=0u,d4_off_count_=0u;
    std::uint16_t cursor_=0u;
    std::uint32_t frame_=0u;
    std::uint8_t pending_group_=2u;
    std::array<bool,2> held_{};
    bool ready_=false;
};
} // namespace awe64::vst3
