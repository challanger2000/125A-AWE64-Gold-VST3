#pragma once
// P002 RESEARCH ONLY: opt-in, source-backed ROM-mono SF2 bank inside the
// actual exported Windows VST3. The original 1996 SHA-pinned SBAWE.VXD
// source resources and the full source-owned initial device state arrive
// through a PRIVATE locally generated binary pack. Never embed, publish,
// or distribute that pack or Creative bank/ROM contents.
//
// Loading and parsing input files happen ONLY on setActive() (non-RT).
// Per-note ROM bank parsing and the original hardware allocator are
// bounded and allocation-free, but this is not yet validated for full
// production-grade 44.1 kHz callback worst-case timing. No fake DSP,
// RAM SoundFont upload, voice stealing, retrigger, or Win95 PCM parity.
#include "CreativeSf2RomBankNoteCompiler.h"
#include "CreativeMidi.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace awe64::vst3 {
class P002PrivateRomBank {
public:
    [[nodiscard]] bool load(const std::filesystem::path& source_pack,
                            const std::filesystem::path& soundfont,
                            std::string& error);
    void clear() noexcept;
    [[nodiscard]] bool ready() const noexcept {return ready_;}
    [[nodiscard]] std::uint8_t channel() const noexcept {return channel_;}
    [[nodiscard]] bool noteOn(awe64::Chip& chip,std::uint8_t channel,
                              std::uint8_t key,std::uint8_t velocity) noexcept;
    [[nodiscard]] bool noteOff(awe64::Chip& chip,std::uint8_t channel,
                               std::uint8_t key) noexcept;
    [[nodiscard]] bool sustain(awe64::Chip& chip,std::uint8_t value) noexcept;
    [[nodiscard]] std::size_t trackedVoices() const noexcept {
        return ownership_.trackedPhysicalVoices();
    }
private:
    std::vector<std::uint8_t> bank_bytes_;
    std::array<std::uint8_t,awe64::creative::kOriginalVxdMinDeviceImage> state_{};
    awe64::creative::OriginalCreativeSf2DriverResources resources_{};
    std::array<std::uint16_t,15> negative_{};
    std::array<std::uint16_t,16> positive_{};
    awe64::creative::OriginalSf2RomBankVoiceOwnership ownership_{};
    awe64::creative::MidiState midi_{};
    std::array<bool,128> held_keys_{};
    std::uint16_t bank_number_=0;
    std::uint8_t channel_=0, program_=0;
    bool ready_=false;
};
} // namespace awe64::vst3
