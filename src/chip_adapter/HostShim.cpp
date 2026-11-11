#include "HostShim.h"
#include <86box/io.h>
#include <86box/sound.h>
#include <86box/snd_emu8k_trace.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
thread_local char rom_file[4096] = {};
}
extern "C" {
AWE64_TLS int wavetable_pos_global = 0;
bool awe64_host_set_rom_path(const char *path) {
    if (!path) return false;
    const auto len = std::strlen(path);
    if (!len || len >= sizeof(rom_file)) return false;
    std::memcpy(rom_file, path, len + 1);
    return true;
}
FILE *rom_fopen(const char *, char *mode) {
    if (!rom_file[0]) return nullptr;
    return std::fopen(rom_file, mode);
}
void fatal(const char *format, ...) {
    std::fputs("Unrecoverable EMU8000 fork initialization failure: ", stderr);
    va_list args; va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::abort(); // Chip::open prevalidates ROM; no silent alternate sound.
}
void pclog_ex(const char *, va_list) {}
void emu8k_trace_advance(int) {}
void emu8k_trace_write(uint16_t, uint16_t) {}
void emu8k_trace_read(uint16_t, uint16_t) {}
void emu8k_trace_wav(const int32_t *, int) {}
void io_sethandler(uint16_t, uint16_t,
    uint8_t (*)(uint16_t,void*), uint16_t (*)(uint16_t,void*),
    uint32_t (*)(uint16_t,void*), void (*)(uint16_t,uint8_t,void*),
    void (*)(uint16_t,uint16_t,void*), void (*)(uint16_t,uint32_t,void*),void*) {}
void io_removehandler(uint16_t, uint16_t,
    uint8_t (*)(uint16_t,void*), uint16_t (*)(uint16_t,void*),
    uint32_t (*)(uint16_t,void*), void (*)(uint16_t,uint8_t,void*),
    void (*)(uint16_t,uint16_t,void*), void (*)(uint16_t,uint32_t,void*),void*) {}
}
