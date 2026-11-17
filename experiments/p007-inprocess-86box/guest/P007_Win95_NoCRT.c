/* Windows95 PE32 guest AWE WinMM relay WITHOUT MSVC runtime.
 * /NODEFAULTLIB /ENTRY:P007GuestMain /SUBSYSTEM:CONSOLE,4.0.
 * Uses only Win95 KERNEL32 and WINMM imports; runtime test still needed.
 */
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <mmsystem.h>
/* Host ABI is 6-byte COM2 message shared with original 86Box UART. */
#define P007_PACKET_MAGIC 0xA5u
#define P007_PACKET_SIZE 6u
/* Keep these byte-for-byte equal to include/P007MidiUart.h. */
#define P007_GUEST_READY_TOKEN "P007-AWE-WINMM-READY-1\n"
#define P007_GUEST_STOP_TOKEN  "P007-AWE-WINMM-STOP-1\n"
static char upper(char c){return c>='a'&&c<='z'?(char)(c-'a'+'A'):c;}
static int contains(char const* s,char const* part){
 unsigned i,j;
 if(!s||!part)return 0;
 for(i=0;s[i];++i){
  for(j=0;part[j]&&s[i+j];++j)
   if(upper(s[i+j])!=upper(part[j]))break;
  if(!part[j])return 1;
 }
 return 0;
}
static void clear(void* p,unsigned n){
 unsigned i;volatile unsigned char* b=(volatile unsigned char*)p;
 for(i=0;i<n;++i)b[i]=0;
}
static void log_line(char const* s){
 DWORD written=0;
 HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE);
 if(h && h!=INVALID_HANDLE_VALUE)
  (void)WriteFile(h,s,(DWORD)lstrlenA(s),&written,NULL);
}
static int select_original(HMIDIOUT* out){
 UINT i,n=midiOutGetNumDevs();MIDIOUTCAPSA cap;
 for(i=0;i<n;++i){
  clear(&cap,(unsigned)sizeof(cap));
  if(midiOutGetDevCapsA(i,&cap,sizeof(cap))!=MMSYSERR_NOERROR)
   continue;
  if(!contains(cap.szPname,"AWE") &&
     !contains(cap.szPname,"EMU8000"))continue;
  if(midiOutOpen(out,i,0,0,CALLBACK_NULL)==MMSYSERR_NOERROR){
   log_line("P007_ORIGINAL_CREATIVE_WINMM_OPEN\r\n");
   return 1;
  }
 }
 log_line("P007_NO_ORIGINAL_AWE_SYNTH (no replacement)\r\n");return 0;
}
static HANDLE open_serial(void){
 HANDLE h;DCB dcb;COMMTIMEOUTS t;
 h=CreateFileA("COM2",GENERIC_READ|GENERIC_WRITE,0,NULL,
  OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 if(h==INVALID_HANDLE_VALUE)return h;
 if(!GetCommState(h,&dcb)){CloseHandle(h);return INVALID_HANDLE_VALUE;}
 dcb.BaudRate=115200;dcb.ByteSize=8;
 dcb.Parity=NOPARITY;dcb.StopBits=ONESTOPBIT;
 dcb.fBinary=TRUE;dcb.fParity=FALSE;
 dcb.fOutxCtsFlow=FALSE;dcb.fOutxDsrFlow=FALSE;
 dcb.fDtrControl=DTR_CONTROL_ENABLE;
 dcb.fRtsControl=RTS_CONTROL_ENABLE;
 dcb.fOutX=FALSE;dcb.fInX=FALSE;
 if(!SetCommState(h,&dcb)){CloseHandle(h);return INVALID_HANDLE_VALUE;}
 clear(&t,(unsigned)sizeof(t));
 t.ReadIntervalTimeout=MAXDWORD;
 t.ReadTotalTimeoutConstant=50;
 t.WriteTotalTimeoutConstant=100;
 if(!SetCommTimeouts(h,&t)){CloseHandle(h);return INVALID_HANDLE_VALUE;}
 return h;
}
static int send_guest_marker(HANDLE port,const char* msg){
 DWORD n=0,expected=(DWORD)lstrlenA(msg);
 return WriteFile(port,msg,expected,&n,NULL)&&n==expected;
}
static int valid(unsigned char const* p){
 unsigned char crc=(unsigned char)(0x5au^p[0]^p[1]^p[2]^p[3]^p[4]);
 return p[0]==P007_PACKET_MAGIC && p[2]>=0x80u && p[2]<0xf0u &&
  p[3]<128u && p[4]<128u && p[5]==crc;
}
static void panic(HMIDIOUT h){
 UINT ch;
 for(ch=0;ch<16u;++ch)
  (void)midiOutShortMsg(h,(DWORD)(0xb0u|ch|(123u<<8)));
}
__declspec(noreturn) void __cdecl P007GuestMain(void){
 HANDLE com=INVALID_HANDLE_VALUE;HMIDIOUT synth=NULL;
 unsigned char bytes[P007_PACKET_SIZE],byte=0;
 unsigned count=0;DWORD got=0;
 log_line("P007 Windows95 original Creative AWE WinMM relay\r\n");
 if(!select_original(&synth))ExitProcess(2);
 for(;;){
  if(com==INVALID_HANDLE_VALUE){
   com=open_serial();
   if(com==INVALID_HANDLE_VALUE){Sleep(200u);continue;}
   count=0;log_line("P007_GUEST_COM2_CONNECTED\r\n");
   /* Guest sends READY only after select_original() succeeded and the
      emulated COM2 UART was opened. No substitute synth is accepted. */
   if(!send_guest_marker(com,P007_GUEST_READY_TOKEN)){
    CloseHandle(com);com=INVALID_HANDLE_VALUE;Sleep(200u);continue;
   }
  }
  got=0;
  if(!ReadFile(com,&byte,1u,&got,NULL)){
   CloseHandle(com);com=INVALID_HANDLE_VALUE;
   panic(synth);continue;
  }
  if(got==0u)continue;
  if(count==0u&&byte!=P007_PACKET_MAGIC)continue;
  bytes[count++]=byte;
  if(count<P007_PACKET_SIZE)continue;
  count=0;
  if(!valid(bytes)){
   unsigned i,j;
   for(i=1u;i<P007_PACKET_SIZE;++i)
    if(bytes[i]==P007_PACKET_MAGIC)break;
   if(i<P007_PACKET_SIZE){
    for(j=i;j<P007_PACKET_SIZE;++j)bytes[j-i]=bytes[j];
    count=P007_PACKET_SIZE-i;
   }
   continue;
  }
  {
   DWORD msg=(DWORD)bytes[2]|((DWORD)bytes[3]<<8)|((DWORD)bytes[4]<<16);
   if(midiOutShortMsg(synth,msg)!=MMSYSERR_NOERROR){
    log_line("P007_ORIGINAL_CREATIVE_WINMM_ERROR\r\n");
    (void)send_guest_marker(com,P007_GUEST_STOP_TOKEN);
    panic(synth);break;
   }
  }
 }
 if(com!=INVALID_HANDLE_VALUE)CloseHandle(com);
 midiOutReset(synth);midiOutClose(synth);ExitProcess(3);
}
