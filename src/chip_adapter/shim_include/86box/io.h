#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void io_sethandler(uint16_t base, uint16_t size,
    uint8_t (*inb)(uint16_t, void*), uint16_t (*inw)(uint16_t, void*),
    uint32_t (*inl)(uint16_t, void*), void (*outb)(uint16_t, uint8_t, void*),
    void (*outw)(uint16_t, uint16_t, void*), void (*outl)(uint16_t, uint32_t, void*),
    void *priv);
void io_removehandler(uint16_t base, uint16_t size,
    uint8_t (*inb)(uint16_t, void*), uint16_t (*inw)(uint16_t, void*),
    uint32_t (*inl)(uint16_t, void*), void (*outb)(uint16_t, uint8_t, void*),
    void (*outw)(uint16_t, uint16_t, void*), void (*outl)(uint16_t, uint32_t, void*),
    void *priv);
#ifdef __cplusplus
}
#endif
