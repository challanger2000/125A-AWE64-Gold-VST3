#include "P007AudioRing.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
static int equal(float a,float b){return fabsf(a-b)<0.00001f;}
int main(void) {
    float out[16] = {0};
    p007_audio_reset_quiescent();
    assert(p007_audio_pull(out,8u)==0u);
    const float f[8]={-1.125f,0.25f, 1.25f,-0.5f, 0.0f,0.5f, 0.75f,1.5f};
    p007_audio_capture_float(f,4u);
    assert(p007_audio_available()==4u);
    assert(p007_audio_pull(out,2u)==2u);
    for(int i=0;i<4;++i)assert(equal(out[i],f[i]));
    assert(p007_audio_pull(out,8u)==2u);
    for(int i=0;i<4;++i)assert(equal(out[i],f[i+4]));
    assert(p007_audio_available()==0u);
    int16_t p[6]={-32768,32767, 0,-16384, 16384,1};
    p007_audio_capture_pcm16(p,3u);
    assert(p007_audio_pull(out,3u)==3u);
    assert(equal(out[0],-1.0f));
    assert(equal(out[1],32767.0f/32768.0f));
    assert(equal(out[3],-0.5f));
    assert(equal(out[4],0.5f));
    p007_audio_reset_quiescent();
    /* Ring wrap, saturation and lost-newest policy. */
    float* frame=(float*)malloc((P007_AUDIO_FRAMES+20u)*2u*sizeof(float));
    assert(frame);
    for(uint32_t i=0;i<P007_AUDIO_FRAMES+20u;++i){
       frame[i*2u]=(float)i;
       frame[i*2u+1u]=-(float)i;
    }
    p007_audio_capture_float(frame,P007_AUDIO_FRAMES+20u);
    assert(p007_audio_available()==P007_AUDIO_FRAMES);
    assert(p007_audio_dropped_frames()==20u);
    float block[2048];
    uint32_t read=0u;
    while(p007_audio_available()){
        uint32_t n=p007_audio_pull(block,1024u);
        assert(n>0u&&n<=1024u);
        for(uint32_t i=0;i<n;++i){
            assert(equal(block[i*2u],(float)(read+i)));
            assert(equal(block[i*2u+1u],-(float)(read+i)));
        }
        read+=n;
    }
    assert(read==P007_AUDIO_FRAMES);
    /* SPSC monotonic counters and zero-allocation callback shape. */
    p007_audio_reset_quiescent();
    p007_audio_capture_float(frame,128u);
    assert(p007_audio_pull(block,64u)==64u);
    p007_audio_capture_float(frame+256u,128u);
    assert(p007_audio_available()==192u);
    free(frame);
    puts("P007_INTERNAL_ORIGINAL_86BOX_WAVETABLE_PCM_RING_PASS");
    return 0;
}
