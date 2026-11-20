#pragma once
/* Source-locked 86Box original wavetable mixer -> internal stereo PCM.
   Single producer: original emulation thread.
   Single consumer: VST3 audio callback.
   No heap, no file I/O, no OS sync/wait in either callback.
   Reset ONLY before emulator/thread start or after both threads stop. */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define P007_AUDIO_FRAMES 65536u
void p007_audio_capture_float(const float *interleaved_stereo, uint32_t frames);
void p007_audio_capture_pcm16(const int16_t *interleaved_stereo, uint32_t frames);
uint32_t p007_audio_pull(float *interleaved_stereo, uint32_t frames);
uint32_t p007_audio_available(void);
/* Called ONLY from the consumer audio callback. If backlog exceeds max,
   discard stale historical frames to keep the most recent retain frames.
   This makes a stopped/suspended DAW resume near-live after the original
   Win95 guest has run for tens of seconds with no host callbacks. SPSC only,
   bounded O(1) and allocation-free, never called on original CPU thread. */
uint32_t p007_audio_trim_backlog(uint32_t max_backlog,uint32_t retain_frames);
uint64_t p007_audio_trimmed_frames(void);
uint64_t p007_audio_dropped_frames(void);
void p007_audio_reset_quiescent(void);
#ifdef __cplusplus
}
#endif
