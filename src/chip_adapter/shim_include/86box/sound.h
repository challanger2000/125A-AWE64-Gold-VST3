#pragma once
#define WT_FREQ 44100
#define WTBUFLEN (WT_FREQ / 45)
#if defined(_MSC_VER)
#define AWE64_TLS __declspec(thread)
#else
#define AWE64_TLS __thread
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Per-audio-thread replacement for the single-VM 86Box global time cursor. */
extern AWE64_TLS int wavetable_pos_global;
#ifdef __cplusplus
}
#endif
