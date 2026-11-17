/* P006 passive 86Box-AWE32 wavetable output hook. No synth rewrite.
 * Compiled as extra source src/sound/P006ForkAudioCapture.c in the
 * owner's separate pinned fork. ORIGINAL snd_emu8k.c remains unmodified.
 */
#include "P006Wire.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
static HANDLE mapping=NULL;
static P006AudioShared* audio=NULL;
static int disabled=0;
static void connect_owner(void) {
    const char* name;
    if(audio||disabled)return;
    name=getenv("AWE64_P006_AUDIO_SHM");
    if(!name||!name[0]){disabled=1;return;}
    mapping=OpenFileMappingA(FILE_MAP_READ|FILE_MAP_WRITE,FALSE,name);
    if(!mapping)return; /* host mapping may be created later; retry */
    audio=(P006AudioShared*)MapViewOfFile(mapping,
           FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(P006AudioShared));
    if(!audio){CloseHandle(mapping);mapping=NULL;return;}
    if(audio->magic!=P006_ABI_MAGIC ||
       audio->version!=P006_ABI_VERSION ||
       audio->rate!=P006_RATE ||
       audio->channels!=P006_CHANNELS ||
       audio->capacity!=P006_AUDIO_CAPACITY ||
       audio->format!=1u) {
        UnmapViewOfFile(audio);audio=NULL;
        CloseHandle(mapping);mapping=NULL;
    }
}
void p006_capture_wavetable(void const* source,int is_float,uint32_t frames) {
    uint32_t wr,rd,occupied,writeCount,f;
    volatile LONG *writer,*reader,*overruns;
    if(!audio)connect_owner();
    if(!audio||!source||frames==0u)return;
    writer=(volatile LONG*)&audio->write_frames;
    reader=(volatile LONG*)&audio->read_frames;
    overruns=(volatile LONG*)&audio->overrun_frames;
    wr=(uint32_t)InterlockedCompareExchange(writer,0,0);
    rd=(uint32_t)InterlockedCompareExchange(reader,0,0);
    occupied=wr-rd;
    if(occupied>P006_AUDIO_CAPACITY)return;
    writeCount=frames;
    if(writeCount>P006_AUDIO_CAPACITY-occupied)
        writeCount=P006_AUDIO_CAPACITY-occupied;
    if(is_float){
        const float* src=(const float*)source;
        for(f=0;f<writeCount;++f){
            uint32_t i=((wr+f)&(P006_AUDIO_CAPACITY-1u))*2u;
            audio->samples[i]=src[f*2u];
            audio->samples[i+1u]=src[f*2u+1u];
        }
    }else{
        const int16_t* src=(const int16_t*)source;
        for(f=0;f<writeCount;++f){
            uint32_t i=((wr+f)&(P006_AUDIO_CAPACITY-1u))*2u;
            audio->samples[i]=(float)src[f*2u]*(1.f/32768.f);
            audio->samples[i+1u]=(float)src[f*2u+1u]*(1.f/32768.f);
        }
    }
    if(writeCount!=0u)
        (void)InterlockedExchange(writer,(LONG)(wr+writeCount));
    if(writeCount<frames)
        (void)InterlockedExchangeAdd(overruns,(LONG)(frames-writeCount));
}
#else
#include <stdint.h>
void p006_capture_wavetable(void const* source,int is_float,uint32_t frames){
    (void)source;(void)is_float;(void)frames;
}
#endif
