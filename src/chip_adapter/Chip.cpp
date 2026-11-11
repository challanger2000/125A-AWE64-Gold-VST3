#include "Chip.h"
#include "HostShim.h"
#include <86box/sound.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <new>
#include <system_error>

extern "C" {
void emu8k_outw(uint16_t addr, uint16_t val, void* priv);
uint16_t emu8k_inw(uint16_t addr, void* priv);
}

namespace awe64 {
void Chip::DeviceDeleter::operator()(emu8k_t* p) const noexcept {
    if (!p) return;
    emu8k_close(p);
    // The pinned fork close() frees ROM/RAM but not the empty placeholder.
    std::free(p->empty);
    delete p;
}
Chip::~Chip() = default;
void Chip::close() noexcept { device_.reset(); }

bool Chip::open(const std::filesystem::path& privateRom, std::string& error) {
    if (device_) { error = "Chip already initialized"; return false; }
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(privateRom, ec);
    if (ec || (bytes != 1048574u && bytes != 1048576u)) {
        error = "Private EMU8000 ROM missing or invalid size (must be 1048574/1048576 bytes)";
        return false;
    }
    std::ifstream verify(privateRom, std::ios::binary);
    if (!verify) { error = "Cannot open private EMU8000 ROM"; return false; }
    const auto utf8 = privateRom.u8string();
    const std::string path(utf8.begin(), utf8.end());
    if (!awe64_host_set_rom_path(path.c_str())) {
        error = "Invalid private ROM path";
        return false;
    }
    auto candidate = std::unique_ptr<emu8k_t, DeviceDeleter>(new (std::nothrow) emu8k_t{});
    if (!candidate) { error = "Unable to allocate chip state"; return false; }
    // 86Box's on-board RAM argument is KiB, not bytes or MiB.
    emu8k_init(candidate.get(), 0, kSampleRamKiB);
    // The original 86Box chip optionally enables EMU8K_STATE_DUMP from the
    // process environment. Its emu8k_update() would then fprintf() from the
    // DAW's real-time audio thread at each dump boundary. Never allow file IO
    // from VST3 rendering, even when an unrelated host/app sets that env var.
    // This compatibility restriction lives OUTSIDE the pinned, unmodified DSP.
    // The original fork's emu8k_close() does not close state_dump either.
    if (candidate->state_dump) {
        std::fclose(static_cast<FILE*>(candidate->state_dump));
        candidate->state_dump = nullptr;
    }
    if (!candidate->rom || !candidate->ram ||
        candidate->ram_end_addr != kSampleRamEnd ||
        candidate->ram_pointers[32] != candidate->ram ||
        candidate->ram_pointers[255] != candidate->ram + (223u * 65536u) ||
        candidate->ram_pointers[31] != candidate->empty) {
        error = "Fork chip RAM/ROM mapping does not match 28 MiB hardware";
        return false;
    }
    device_ = std::move(candidate);
    error.clear();
    return true;
}
bool Chip::uploadRam(uint32_t address, std::span<const int16_t> samples) noexcept {
    if (!device_) return false;
    if (address < kSampleRamBase || address >= kFmReservedBase) return false;
    if (samples.size() > static_cast<std::size_t>(kFmReservedBase - address)) return false;
    if (!samples.empty())
        std::memcpy(device_->ram + (address - kSampleRamBase),
                    samples.data(), samples.size_bytes());
    return true;
}
void Chip::writePort(uint16_t port, uint16_t value) noexcept {
    if (!device_) return;
    wavetable_pos_global = device_->pos;
    emu8k_outw(port, value, device_.get());
}
uint16_t Chip::readPort(uint16_t port) noexcept {
    return device_ ? emu8k_inw(port, device_.get()) : 0xffffu;
}
void Chip::render(int32_t* dest, std::size_t frames) noexcept {
    if (!device_ || !dest) return;
    while (frames) {
        const auto n = std::min<std::size_t>(frames, WTBUFLEN);
        emu8k_reset_buffer(device_.get());
        wavetable_pos_global = static_cast<int>(n);
        emu8k_update(device_.get());
        std::memcpy(dest, device_->buffer, n * 2u * sizeof(int32_t));
        dest += n * 2u; frames -= n;
    }
}
}
