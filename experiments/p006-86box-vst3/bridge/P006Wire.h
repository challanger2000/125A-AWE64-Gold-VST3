#pragma once
// Versioned native 86Box -> VST3 audio ABI. No Creative/VXD/ROM/SF2 data.
#include <stdint.h>
#include <stddef.h>
#define P006_ABI_MAGIC 0x36584541u
#define P006_ABI_VERSION 1u
#define P006_RATE 44100u
#define P006_CHANNELS 2u
#define P006_AUDIO_CAPACITY 32768u
#define P006_PACKET_MAGIC 0xA5u
#define P006_PACKET_SIZE 6u
#ifdef __cplusplus
extern "C" {
#endif
typedef struct P006AudioShared {
    uint32_t magic,version,rate,channels,capacity,format;
    volatile int32_t write_frames,read_frames,overrun_frames,underrun_frames;
    uint32_t reserved[6];
    float samples[P006_AUDIO_CAPACITY*P006_CHANNELS];
} P006AudioShared;
typedef struct P006MidiPacket {
    uint8_t magic,seq,status,d1,d2,crc;
} P006MidiPacket;
#ifdef __cplusplus
}
static_assert(sizeof(P006MidiPacket)==P006_PACKET_SIZE);
static_assert((offsetof(P006AudioShared,samples)%4u)==0u);
static_assert((P006_AUDIO_CAPACITY&(P006_AUDIO_CAPACITY-1u))==0u);
#endif
