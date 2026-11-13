#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
#include <86box/sound.h>
#include <86box/snd_emu8k.h>
}
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace awe64 {
inline constexpr int kChipHz = 44100;
inline constexpr int kSampleRamKiB = 28672;
inline constexpr std::size_t kSampleRamBytes = 28u * 1024u * 1024u;
inline constexpr uint32_t kSampleRamBase = 0x200000u;
inline constexpr uint32_t kSampleRamEnd = 0x1000000u;
inline constexpr uint32_t kFmReservedBase = 0xffffe0u;

// Source-independent native VST3 adapter telemetry. Each event is a
// literal 16-bit port OUT forwarded to the ORIGINAL pinned EMU8000 core.
// The sample clock counts rendered 44.1kHz frames. This cannot establish
// Creative Win95 driver provenance on its own.
struct ChipPortWrite {
    std::uint64_t frame=0;
    std::uint16_t port=0;
    std::uint16_t value=0;
};
class Chip final {
public:
    Chip() = default;
    ~Chip();
    Chip(const Chip&) = delete;
    Chip& operator=(const Chip&) = delete;

    // Non-realtime: checks format, initializes the unmodified 86Box core.
    bool open(const std::filesystem::path& privateRom, std::string& error);
    void close() noexcept;

    // Non-realtime control/write path only; not safe concurrent with render().
    bool uploadRam(uint32_t emuWordAddress, std::span<const int16_t> samples) noexcept;
    void writePort(uint16_t port, uint16_t value) noexcept;
    uint16_t readPort(uint16_t port) noexcept;

    // Allocate every slot off-RT. writePort()/render() only perform
    // bounded assignments, NO heap, locks, filesystem or logging.
    [[nodiscard]] bool armPortWriteTrace(std::size_t maximum_writes) noexcept;
    void disarmPortWriteTrace() noexcept { trace_armed_=false; }
    [[nodiscard]] std::span<const ChipPortWrite> portWriteTrace() const noexcept {
        return {trace_buffer_.data(),trace_count_};
    }
    [[nodiscard]] bool portWriteTraceOverflow() const noexcept {
        return trace_overflow_;
    }
    [[nodiscard]] std::uint64_t renderedFrames() const noexcept {
        return rendered_frames_;
    }

    // Realtime when initialized: bounded chunks, no heap/locks/file I/O.
    // Output is original fork signed 32-bit interleaved digital samples, not normalized.
    void render(int32_t* interleavedStereo, std::size_t frames) noexcept;
    [[nodiscard]] emu8k_t* rawForContractTesting() noexcept { return device_.get(); }
    [[nodiscard]] const emu8k_t* rawForContractTesting() const noexcept { return device_.get(); }
    [[nodiscard]] bool ready() const noexcept { return static_cast<bool>(device_); }
private:
    struct DeviceDeleter { void operator()(emu8k_t* p) const noexcept; };
    std::unique_ptr<emu8k_t, DeviceDeleter> device_;
    std::vector<ChipPortWrite> trace_buffer_{};
    std::size_t trace_count_=0;
    bool trace_armed_=false, trace_overflow_=false;
    std::uint64_t rendered_frames_=0;
};
}
