/* P007BE deterministic exact-byte C transport test; ORIGINAL 86Box/Win95
 * AUDIO is NOT part of this test. 8-bit control + 32-bit payload callbacks.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned installs=0u,removes=0u,resets=0u;
void io_sethandler(uint16_t a,uint16_t b,
 uint8_t(*c)(uint16_t,void*),uint16_t(*d)(uint16_t,void*),
 uint32_t(*e)(uint16_t,void*),void(*f)(uint16_t,uint8_t,void*),
 void(*g)(uint16_t,uint16_t,void*),void(*h)(uint16_t,uint32_t,void*),void*i){
 (void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;
 if(a==0xeb00u && b==16u)++installs;
}
void io_removehandler(uint16_t a,uint16_t b,
 uint8_t(*c)(uint16_t,void*),uint16_t(*d)(uint16_t,void*),
 uint32_t(*e)(uint16_t,void*),void(*f)(uint16_t,uint8_t,void*),
 void(*g)(uint16_t,uint16_t,void*),void(*h)(uint16_t,uint32_t,void*),void*i){
 (void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;
 if(a==0xeb00u && b==16u)++removes;
}
void p007_midi_guest_reset_sf2_ack(void){++resets;}
#include "../fork/P007LiveSf2.c"
static void check(int ok,const char *msg){
 if(!ok){fprintf(stderr,"P007BE_TRANSPORT_FAIL %s\n",msg);exit(2);}
}
int main(void){
 enum{N=315982}; /* exact byte count of owner bank, NOT private content */
 uint8_t *original=(uint8_t*)malloc(N),*received=(uint8_t*)malloc(N);
 check(original!=NULL && received!=NULL,"allocation");
 for(unsigned i=0u;i<N;++i)original[i]=(uint8_t)(i*37u+13u);
 memcpy(original,"RIFF",4);
 {uint32_t n=N-8u;for(unsigned i=0u;i<4u;++i)
     original[4u+i]=(uint8_t)(n>>(i*8u));}
 memcpy(original+8,"sfbk",4);
 check(p007_live_soundfont_publish(original,N)==-1,
       "cannot publish to unattached guest");
 p007_live_soundfont_attach();
 check(installs==1u,"I/O registration");
 check(p007_live_soundfont_publish_count()==0u &&
       p007_live_soundfont_complete_count()==0u,"initial counter reset");
 check(p007_live_soundfont_publish(original,N)==1,"publish real bytes");
 check(resets==1u,"must clear stale original Creative ack");
 check(p007_live_soundfont_publish_count()==1u &&
       p007_live_soundfont_complete_count()==0u,"first generation not published");
 check(p007_live_in8(0xeb00u,NULL)==0xa7u,"guest pending magic");
 uint32_t n=0u;
 for(unsigned i=0u;i<4u;++i)
   n|=(uint32_t)p007_live_in8((uint16_t)(0xeb02u+i),NULL)<<(8u*i);
 check(n==N,"exact file length transmitted");
 p007_live_out8(0xeb06u,1u,NULL);
 check(p007_live_soundfont_state()==2,"guest acknowledged begin");
 check(p007_live_soundfont_publish(original,N)==0,
       "must not overwrite in-flight original x86 transfer");
 for(unsigned pos=0u;pos<N;pos+=4u) {
   uint32_t word=p007_live_in32(0xeb08u,NULL);
   for(unsigned k=0u;k<4u && pos+k<N;++k)
      received[pos+k]=(uint8_t)(word>>(8u*k));
 }
 check(memcmp(original,received,N)==0,"every guest SF2 byte exact");
 check(p007_live_soundfont_state()==2,"not done until guest AWE ack");
 p007_live_out8(0xeb06u,2u,NULL);
 check(p007_live_soundfont_state()==3,"guest success ack accepted");
 check(p007_live_soundfont_complete_count()==1u &&
       p007_live_soundfont_failure_count()==0u,"guest completion generation lost");
 original[500]^=0x5au;
 check(p007_live_soundfont_publish(original,N)==1,
       "same-size second SF2 must be accepted without guest reboot");
 check(resets==2u,"second Creative ACK must be retired");
 check(p007_live_soundfont_publish_count()==2u &&
       p007_live_soundfont_complete_count()==1u,"second publish not independent");
 p007_live_out8(0xeb06u,1u,NULL);
 p007_live_out8(0xeb06u,3u,NULL);
 check(p007_live_soundfont_state()==4,"guest fail must not pretend ready");
 check(p007_live_soundfont_failure_count()==1u &&
       p007_live_soundfont_complete_count()==1u,"failed guest counted as successful");
 original[0]='X';
 check(p007_live_soundfont_publish(original,N)==-2,
       "invalid RIFF must never reach original Win95");
 p007_live_soundfont_detach();
 check(removes==1u,"I/O device removed");
 check(p007_live_soundfont_publish(received,N)==-1,
       "cannot load after original guest closed");
 free(original);free(received);
 puts("P007BE_REAL_SF2_BYTECOUNT_SYNTHETIC_PIO_TRANSFER_RELOAD_FAILCLOSED_UNIT_PASS");
 puts("P007BE_NOT_A_PRIVATE_WIN95_CREATIVE_ORIGINAL_PCM_TEST");
 return 0;
}
