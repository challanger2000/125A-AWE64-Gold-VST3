#pragma once
/* P007BE private in-guest file transport (NOT a synth). */
#include <stdint.h>
#define P007_LIVE_SF2_BASE 0xeb00u
#define P007_LIVE_SF2_MAX_BYTES (28u*1024u*1024u)
#define P007_LIVE_SF2_READY_MAGIC 0xa7u
/* state 0 idle, 1 pending, 2 consuming, 3 done, 4 guest failed */
int p007_live_soundfont_publish(const void *data,uint32_t size);
int p007_live_soundfont_state(void);
/* Passive atomic counters: distinguish NEW guest transfers from stale ACKs. */
uint32_t p007_live_soundfont_publish_count(void);
uint32_t p007_live_soundfont_complete_count(void);
uint32_t p007_live_soundfont_failure_count(void);
void p007_live_soundfont_attach(void);
void p007_live_soundfont_detach(void);
