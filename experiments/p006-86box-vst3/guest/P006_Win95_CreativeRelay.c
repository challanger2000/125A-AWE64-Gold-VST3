/* Private Windows 95 guest ONLY: serial COM2 -> original Creative AWE
 * WinMM midiOutShortMsg. No Creative driver, ROM or SoundFont is included.
 * Build with an actual Windows 95 compatible Win32 C toolchain.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#define P006_MAGIC 0xA5u
#define P006_SIZE 6u
static int contains_ci(const char *s,const char *needle){
 size_t i,j,n=strlen(needle);
 for(i=0;s[i];++i){
  for(j=0;j<n&&s[i+j];++j)
   if(toupper((unsigned char)s[i+j])!=toupper((unsigned char)needle[j]))break;
  if(j==n)return 1;
 }
 return 0;
}
static int open_original_creative(HMIDIOUT *out){
 UINT i,n=midiOutGetNumDevs();
 MIDIOUTCAPSA cap;
 for(i=0;i<n;++i){
  memset(&cap,0,sizeof(cap));
  if(midiOutGetDevCapsA(i,&cap,sizeof(cap))!=MMSYSERR_NOERROR)continue;
  printf("P006_MIDI_DEVICE %u: %s\n",i,cap.szPname);
  if(!contains_ci(cap.szPname,"AWE") &&
     !contains_ci(cap.szPname,"EMU8000"))continue;
  if(midiOutOpen(out,i,0,0,CALLBACK_NULL)==MMSYSERR_NOERROR){
   printf("P006_ORIGINAL_CREATIVE_WINMM_OPEN: %s\n",cap.szPname);
   return 1;
  }
 }
 puts("P006_ERROR_NO_ORIGINAL_CREATIVE_AWE_WINMM_DEVICE");
 return 0; /* NEVER silently choose Microsoft synth or MPU-401 */
}
static int open_com(const char *port,HANDLE *out){
 HANDLE h;
 DCB dcb;COMMTIMEOUTS timeouts;
 h=CreateFileA(port,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,
               FILE_ATTRIBUTE_NORMAL,NULL);
 if(h==INVALID_HANDLE_VALUE)return 0;
 if(!GetCommState(h,&dcb)){CloseHandle(h);return 0;}
 dcb.BaudRate=115200;dcb.ByteSize=8;
 dcb.Parity=NOPARITY;dcb.StopBits=ONESTOPBIT;
 dcb.fBinary=TRUE;dcb.fParity=FALSE;
 dcb.fOutxCtsFlow=FALSE;dcb.fOutxDsrFlow=FALSE;
 dcb.fDtrControl=DTR_CONTROL_ENABLE;
 dcb.fRtsControl=RTS_CONTROL_ENABLE;
 dcb.fOutX=FALSE;dcb.fInX=FALSE;
 if(!SetCommState(h,&dcb)){CloseHandle(h);return 0;}
 memset(&timeouts,0,sizeof(timeouts));
 timeouts.ReadIntervalTimeout=MAXDWORD;
 timeouts.ReadTotalTimeoutConstant=50;
 timeouts.WriteTotalTimeoutConstant=100;
 if(!SetCommTimeouts(h,&timeouts)){CloseHandle(h);return 0;}
 *out=h;return 1;
}
static int packet_valid(unsigned char p[P006_SIZE]){
 unsigned char crc=(unsigned char)(0x5a^p[0]^p[1]^p[2]^p[3]^p[4]);
 return p[0]==P006_MAGIC && p[2]>=0x80 && p[2]<0xf0 &&
   p[3]<128 && p[4]<128 && p[5]==crc;
}
static void panic_notes_off(HMIDIOUT synth){
 UINT c;
 for(c=0;c<16u;++c)(void)midiOutShortMsg(synth,
       (DWORD)(0xb0u|c|(123u<<8)));
}
int main(int argc,char **argv){
 HMIDIOUT synth=NULL;HANDLE port=INVALID_HANDLE_VALUE;
 unsigned char packet[P006_SIZE],byte=0;
 unsigned count=0,lastSeq=0,seqKnown=0,received=0,invalid=0,lost=0;
 DWORD readCount=0;
 const char *serial=argc>1?argv[1]:"COM2";
 puts("P006 Win95 Original Creative AWE Synth MIDI relay");
 if(!open_original_creative(&synth))return 2;
 while(1){
  if(port==INVALID_HANDLE_VALUE){
   if(!open_com(serial,&port)){Sleep(250);continue;}
   printf("P006_GUEST_SERIAL_CONNECTED %s\n",serial);
   count=0;seqKnown=0;
  }
  readCount=0;
  if(!ReadFile(port,&byte,1u,&readCount,NULL)){
   printf("P006_COM_DISCONNECTED error=%lu\n",GetLastError());
   CloseHandle(port);port=INVALID_HANDLE_VALUE;
   panic_notes_off(synth);continue;
  }
  if(readCount==0u)continue;
  if(count==0u&&byte!=P006_MAGIC)continue;
  packet[count++]=byte;
  if(count<P006_SIZE)continue;
  count=0;
  if(!packet_valid(packet)){
   unsigned i,j;
   ++invalid;
   for(i=1;i<P006_SIZE;++i)if(packet[i]==P006_MAGIC)break;
   if(i<P006_SIZE){
    for(j=i;j<P006_SIZE;++j)packet[j-i]=packet[j];
    count=P006_SIZE-i;
   }
   continue;
  }
  if(seqKnown && (unsigned char)(lastSeq+1u)!=packet[1])++lost;
  lastSeq=packet[1];seqKnown=1;
  {
   const DWORD midi=(DWORD)packet[2]|
       ((DWORD)packet[3]<<8)|((DWORD)packet[4]<<16);
   MMRESULT result=midiOutShortMsg(synth,midi);
   if(result!=MMSYSERR_NOERROR){
    printf("P006_ORIGINAL_CREATIVE_WINMM_ERROR %u\n",(unsigned)result);
    panic_notes_off(synth);break;
   }
   ++received;
   if(received==1u||received%512u==0u)
    printf("P006_ORIGINAL_CREATIVE_WINMM_SENT %u lost=%u invalid=%u\n",
           received,lost,invalid);
  }
 }
 if(port!=INVALID_HANDLE_VALUE)CloseHandle(port);
 midiOutReset(synth);midiOutClose(synth);
 return 3;
}
