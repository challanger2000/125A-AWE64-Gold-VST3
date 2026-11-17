#pragma once
/* In-process host VST3 MIDI -> original emulated COM2 UART -> guest Win95
   Creative MIDI WinMM relay. No host Named Pipe/COM2 or helper process.
   Single producer (host VST3 audio callback), single consumer
   (original emulator serial_receive_timer). */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define P007_MIDI_QUEUE_BYTES 4096u
#define P007_MIDI_WIRE_BYTES 6u
#define P007_MIDI_PACKET_SYNC 0xA5u
/* Only run when guest COM2 is assigned to this internal port. */
void p007_midi_uart_enable(int active);
int p007_midi_uart_active(void);
/* 1 if accepted in full; 0 if not ready/full/bad MIDI. RT-safe. */
int p007_midi_submit(uint8_t status,uint8_t data1,uint8_t data2);
/* Read by the original 86Box emulation/serial timer thread. 1/0. */
int p007_midi_uart_take_byte(uint8_t *out);
uint32_t p007_midi_uart_queued_bytes(void);
uint64_t p007_midi_dropped_packets(void);
/* Reset ONLY when both producer and consumer threads are stopped. */
void p007_midi_reset_quiescent(void);
#ifdef __cplusplus
}
#endif
