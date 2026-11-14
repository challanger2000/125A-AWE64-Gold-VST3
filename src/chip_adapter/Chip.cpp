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
void Chip::close() noexcept {
    device_.reset();
    trace_armed_=false;
    native_note_capture_active_=false;
    native_note_chip_snapshot_.clear();
    native_note_write_count_=0u;
    native_note_write_overflow_=false;
}

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
    rendered_frames_=0;
    trace_count_=0;
    trace_overflow_=false;
    trace_armed_=false;
    error.clear();
    return true;
}
bool Chip::armNativeNoteOutTransaction() noexcept {
    if(!device_ || native_note_capture_active_)return false;
    try {
        native_note_chip_snapshot_.resize(sizeof(emu8k_t));
    }catch (...) {
        native_note_chip_snapshot_.clear();
        return false;
    }
    native_note_write_count_=0u;
    native_note_write_overflow_=false;
    return true;
}
bool Chip::beginNativeNoteOutTransaction() noexcept {
    if(!device_ || native_note_capture_active_ ||
       native_note_chip_snapshot_.size()!=sizeof(emu8k_t))
        return false;
    std::memcpy(native_note_chip_snapshot_.data(),device_.get(),
                sizeof(emu8k_t));
    native_note_old_wavetable_pos_=wavetable_pos_global;
    native_note_write_count_=0u;
    native_note_write_overflow_=false;
    native_note_selector_=0u;
    native_note_capture_active_=true;
    return true;
}
bool Chip::endNativeNoteOutTransaction() noexcept {
    if(!device_ || !native_note_capture_active_)return false;
    // The original ROM/RAM pointers are not replaced, just the C
    // struct's register/voice/channel state. This path only permits
    // NOTE register writes; the sample ROM/RAM was never modified.
    std::memcpy(device_.get(),native_note_chip_snapshot_.data(),
                sizeof(emu8k_t));
    wavetable_pos_global=native_note_old_wavetable_pos_;
    native_note_capture_active_=false;
    return !native_note_write_overflow_;
}
bool Chip::uploadRam(uint32_t address, std::span<const int16_t> samples) noexcept {
    if (!device_ || native_note_capture_active_) return false;
    if (address < kSampleRamBase || address >= kFmReservedBase) return false;
    if (samples.size() > static_cast<std::size_t>(kFmReservedBase - address)) return false;
    if (!samples.empty())
        std::memcpy(device_->ram + (address - kSampleRamBase),
                    samples.data(), samples.size_bytes());
    return true;
}
bool Chip::armPortWriteTrace(std::size_t maxWrites) noexcept {
    if(!device_ || maxWrites==0u || maxWrites>262144u)
        return false;
    // Full reservation on activation thread. No allocation in process().
    try {
        trace_buffer_.resize(maxWrites);
    }catch (...) {
        return false;
    }
    trace_count_=0u;
    trace_overflow_=false;
    trace_armed_=true;
    return true;
}
void Chip::writePort(uint16_t port, uint16_t value) noexcept {
    if (!device_) return;
    if(native_note_capture_active_) {
        // Fail closed BEFORE touching the original chip if this note
        // tries to issue a DRAM upload. SMALW/SMLD use E22 index
        // 0x36/0x3a with DATA1 A20/A22. The snapshot copies register
        // struct only, NOT 28 MiB sample RAM; permitting these writes
        // would silently corrupt already authenticated original samples.
        //
        // E22 0x36/0x3a remain legal selectors for DATA3 on ordinary
        // music voices 22/26; only the DRAM DATA1 combination is banned.
        const bool allowedPort=
            port==0xe22u || port==0xe20u ||
            port==0xa20u || port==0xa22u ||
            port==0x620u || port==0x622u;
        // The pinned original fork maps DATA1 register 1 voices
        // 20..31 to SMALR/SMARR/SMALW/SMARW/SMLD and hardware
        // configuration. Any DATA1 access there is forbidden in a
        // speculative NOTE transaction; legal DATA3 reads/writes on
        // voice 22/26 must remain supported.
        const bool dramOrGlobalData=
            ((native_note_selector_>>5u)==1u) &&
            ((native_note_selector_&31u)>=20u) &&
            (port==0xa20u || port==0xa22u);
        if(native_note_write_overflow_ || !allowedPort ||
           (port==0xe22u && value>0xffu) || dramOrGlobalData ||
           native_note_write_count_>=native_note_writes_.size()) {
            native_note_write_overflow_=true;
            return;
        }
        if(port==0xe22u)native_note_selector_=value;
        native_note_writes_[native_note_write_count_++]={
            rendered_frames_,port,value};
    }else if(trace_armed_) {
        if(trace_count_<trace_buffer_.size())
            trace_buffer_[trace_count_++]={rendered_frames_,port,value};
        else
            trace_overflow_=true;
    }
    wavetable_pos_global = device_->pos;
    emu8k_outw(port, value, device_.get());
}
uint16_t Chip::readPort(uint16_t port) noexcept {
    if(!device_)return 0xffffu;
    if(native_note_capture_active_) {
        // In the pinned original fork, reading E22 advances a GLOBAL
        // random_helper outside emu8k_t; DATA1 global RAM-register
        // reads can mutate the GLOBAL DMA flags/read cursor. None of
        // these effects can be reverted by the C-struct snapshot.
        // Original VXD idle allocator reads 620 (VTFT) only.
        const bool globalRamRead=
            ((native_note_selector_>>5u)==1u) &&
            ((native_note_selector_&31u)>=20u) &&
            (port==0xa20u || port==0xa22u);
        const bool allowedRead=
            port==0xe20u || port==0xa20u || port==0xa22u ||
            port==0x620u || port==0x622u;
        if(port==0xe22u || globalRamRead || !allowedRead) {
            native_note_write_overflow_=true;
            return 0xffffu;
        }
    }
    return emu8k_inw(port,device_.get());
}
void Chip::render(int32_t* dest, std::size_t frames) noexcept {
    if (!device_ || !dest || native_note_capture_active_) return;
    while (frames) {
        const auto n = std::min<std::size_t>(frames, WTBUFLEN);
        emu8k_reset_buffer(device_.get());
        wavetable_pos_global = static_cast<int>(n);
        emu8k_update(device_.get());
        std::memcpy(dest, device_->buffer, n * 2u * sizeof(int32_t));
        dest += n * 2u; frames -= n;
        rendered_frames_+=n;
    }
}
}
