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
/* Research diagnostics: report each failing COM2 Win32 API exactly once.
 * These are the original Windows95 error codes, not synthetic status.
 * No CRT dependency and no additional host process. */
static unsigned s_com_stage_logged=0u;
static DWORD s_com_create_error=0u;
static void log_com_error(unsigned stage,char const* label,DWORD err){
 char hex[]="0x00000000";
 static const char digits[]="0123456789ABCDEF";
 unsigned i;
 if(stage>=8u || (s_com_stage_logged & (1u<<stage)))return;
 s_com_stage_logged |= 1u<<stage;
 for(i=0u;i<8u;++i)hex[2u+i]=digits[(err>>(28u-4u*i))&15u];
 log_line(label);log_line(hex);log_line("\r\n");
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
 if(h==INVALID_HANDLE_VALUE){
  s_com_create_error=GetLastError();
  log_com_error(0u,"P007_COM2_CREATEFILE_ERR=",s_com_create_error);
  return h;
 }
 if(!GetCommState(h,&dcb)){
  log_com_error(1u,"P007_COM2_GETCOMMSTATE_ERR=",GetLastError());
  CloseHandle(h);return INVALID_HANDLE_VALUE;
 }
 dcb.BaudRate=115200;dcb.ByteSize=8;
 dcb.Parity=NOPARITY;dcb.StopBits=ONESTOPBIT;
 dcb.fBinary=TRUE;dcb.fParity=FALSE;
 dcb.fOutxCtsFlow=FALSE;dcb.fOutxDsrFlow=FALSE;
 dcb.fDtrControl=DTR_CONTROL_ENABLE;
 dcb.fRtsControl=RTS_CONTROL_ENABLE;
 dcb.fOutX=FALSE;dcb.fInX=FALSE;
 if(!SetCommState(h,&dcb)){
  log_com_error(2u,"P007_COM2_SETCOMMSTATE_ERR=",GetLastError());
  CloseHandle(h);return INVALID_HANDLE_VALUE;
 }
 clear(&t,(unsigned)sizeof(t));
 t.ReadIntervalTimeout=MAXDWORD;
 t.ReadTotalTimeoutConstant=50;
 t.WriteTotalTimeoutConstant=100;
 if(!SetCommTimeouts(h,&t)){
  log_com_error(3u,"P007_COM2_SETTIMEOUTS_ERR=",GetLastError());
  CloseHandle(h);return INVALID_HANDLE_VALUE;
 }
 return h;
}
/* Win95 may expose a physical 16550 COM2 in BIOS without a VCOMM
 * CreateFile("COM2") device (ERROR_FILE_NOT_FOUND). ONLY for that case
 * communicate via the ORIGINAL virtual 16550 hardware ports as a guest
 * x86 program; this is not a replacement synthesizer or host COM port.
 * Physical x86 I/O is emitted by the CPU inside 86Box's Win95 guest. */
#define P007_UART_BASE 0x02f8u
static void uart_out(unsigned short port,unsigned char value){
 __asm {
  mov dx,port
  mov al,value
  out dx,al
 }
}
static unsigned char uart_in(unsigned short port){
 unsigned char result;
 __asm {
  mov dx,port
  in al,dx
  mov result,al
 }
 return result;
}
static void uart_init_raw(void){
 uart_out((unsigned short)(P007_UART_BASE+1u),0u);    /* no IRQ */
 uart_out((unsigned short)(P007_UART_BASE+3u),0x80u); /* DLAB */
 uart_out((unsigned short)(P007_UART_BASE+0u),1u);    /* 115200 */
 uart_out((unsigned short)(P007_UART_BASE+1u),0u);
 uart_out((unsigned short)(P007_UART_BASE+3u),3u);    /* 8N1 */
 uart_out((unsigned short)(P007_UART_BASE+2u),7u);    /* FIFO */
 uart_out((unsigned short)(P007_UART_BASE+4u),0x0bu); /* DTR RTS OUT2 */
}
static int uart_send_raw(char const* msg){
 unsigned i,j;
 for(i=0u;msg[i];++i){
  for(j=0u;j<1000u;++j){
   if(uart_in((unsigned short)(P007_UART_BASE+5u))&0x20u)break;
   Sleep(1u);
  }
  if(j==1000u)return 0;
  uart_out((unsigned short)P007_UART_BASE,(unsigned char)msg[i]);
 }
 return 1;
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
/* P007AU: ORIGINAL installed Creative Windows-95 AWEMAN32 API.
 * A separate private FAT16 IDE slave exposes D:\P007FONT.SF2. This is
 * NOT a SoundFont parser / replacement synth in the host VST3.
 * Exact published Creative ABI: AWE_OPEN=101, AWE_LOAD_USER_BANK=204,
 * 14-byte CBufferObject (#pragma pack 2), OPER_FILE=0, user bank 1.
 * Do not emit a successful SF2 acknowledgment until AWEManager returns 0.
 * The guest never modifies the original Win95 HDD or external host files.
 */
#define P007_SF2_OK_TOKEN "P007-AWE-SF2-OK-1\n"
#define P007_SF2_FAIL_TOKEN "P007-AWE-SF2-FAIL-1\n"
typedef LONG (WINAPI *P007_CREATIVE_MANAGER)(DWORD,UINT,LPARAM,LPARAM);
#pragma pack(push,2)
typedef struct {
 DWORD m_Size;
 LPSTR m_Buffer;
 DWORD m_SizeUsed;
 WORD m_Flag;
} P007_CREATIVE_FILE_BUFFER;
#pragma pack(pop)
typedef char p007_creative_abi_assert[
    (sizeof(P007_CREATIVE_FILE_BUFFER)==14u)?1:-1];
static HMODULE s_creative_sf2_dll=0;
static P007_CREATIVE_MANAGER s_creative_manager=0;
static DWORD s_creative_handle=0u;
static int p007_load_original_creative_sf2(void){
 static const char path[]="D:\\P007FONT.SF2";
 char filename[128]="D:\\P007FONT.SF2";
 unsigned char riff[12];DWORD n=0,size=0;
 HANDLE file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,
                         NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
 P007_CREATIVE_FILE_BUFFER b;
 LONG rc;
 if(file==INVALID_HANDLE_VALUE)return 0; /* guest ROM-only */
 size=GetFileSize(file,NULL);
 if(size<96u||size>28u*1024u*1024u||
    !ReadFile(file,riff,12u,&n,NULL)||n!=12u||
    riff[0]!='R'||riff[1]!='I'||riff[2]!='F'||riff[3]!='F'||
    riff[8]!='s'||riff[9]!='f'||riff[10]!='b'||riff[11]!='k'||
    ((DWORD)riff[4]|((DWORD)riff[5]<<8u)|
     ((DWORD)riff[6]<<16u)|((DWORD)riff[7]<<24u))!=size-8u) {
    CloseHandle(file);
    log_line("P007_GUEST_SF2_INVALID_FILE\r\n");return -1;
 }
 CloseHandle(file);
 s_creative_sf2_dll=LoadLibraryA("AWEMAN32.DLL");
 if(!s_creative_sf2_dll) {
    log_line("P007_GUEST_SF2_ORIGINAL_AWEMAN32_MISSING\r\n");
    return -1;
 }
 s_creative_manager=(P007_CREATIVE_MANAGER)
     GetProcAddress(s_creative_sf2_dll,"AWEManager");
 if(!s_creative_manager) {
    log_line("P007_GUEST_SF2_ORIGINAL_AWEMANAGER_MISSING\r\n");
    FreeLibrary(s_creative_sf2_dll);s_creative_sf2_dll=0;return -1;
 }
 rc=s_creative_manager(0u,101u,(LPARAM)&s_creative_handle,0);
 if(rc!=0 || s_creative_handle==0u) {
    log_line("P007_GUEST_SF2_ORIGINAL_AWE_OPEN_FAILED\r\n");
    FreeLibrary(s_creative_sf2_dll);s_creative_sf2_dll=0;return -1;
 }
 b.m_Size=sizeof(filename);
 b.m_Buffer=filename;
 b.m_SizeUsed=0u;
 b.m_Flag=0u; /* OPER_FILE */
 rc=s_creative_manager(s_creative_handle,204u,1u,(LPARAM)&b);
 if(rc!=0) {
    log_line("P007_GUEST_SF2_ORIGINAL_AWE_LOAD_USER_BANK_FAILED\r\n");
    (void)s_creative_manager(s_creative_handle,102u,0,0);
    s_creative_handle=0u;
    FreeLibrary(s_creative_sf2_dll);s_creative_sf2_dll=0;return -1;
 }
 log_line("P007_GUEST_SF2_ORIGINAL_CREATIVE_API_LOADED\r\n");
 return 1;
}

__declspec(noreturn) void __cdecl P007GuestMain(void){
 HANDLE com=INVALID_HANDLE_VALUE;HMIDIOUT synth=NULL;
 int raw_mode=0,connected=0, sf2_result=0;
 unsigned char bytes[P007_PACKET_SIZE],byte=0;
 unsigned count=0;DWORD got=0;
 log_line("P007 Windows95 original Creative AWE WinMM relay\r\n");
 if(!select_original(&synth))ExitProcess(2);
 sf2_result=p007_load_original_creative_sf2();
 /* First request Win95 VCOMM port ownership. If that original driver
  * reports ERROR_FILE_NOT_FOUND, use the real 16550 x86 port I/O fallback,
  * as proven by the first full original Creative->EMU8000 PCM test.
  * Direct ring-3 I/O before a VCOMM request is unsafe on Win95. */
 for(;;){
  if(!connected){
   if(!raw_mode){
    com=open_serial();
    if(com==INVALID_HANDLE_VALUE){
     if(s_com_create_error!=ERROR_FILE_NOT_FOUND){
      Sleep(200u);continue;
     }
     raw_mode=1;
     uart_init_raw();
     log_line("P007_COM2_RAW_UART_FALLBACK_16550\r\n");
    }
   }
   count=0;
   if(raw_mode){
    if(sf2_result!=0 && !uart_send_raw(sf2_result>0?
        P007_SF2_OK_TOKEN:P007_SF2_FAIL_TOKEN)) {
     log_line("P007_GUEST_SF2_STATUS_SEND_FAILED\r\n");
     Sleep(200u);continue;
    }
    if(!uart_send_raw(P007_GUEST_READY_TOKEN)){
     log_line("P007_RAW_UART_READY_SEND_FAILED\r\n");
     Sleep(200u);continue;
    }
    log_line("P007_GUEST_RAW_UART_READY_SENT\r\n");
   }else{
    log_line("P007_GUEST_COM2_CONNECTED\r\n");
    if(sf2_result!=0 && !send_guest_marker(com,sf2_result>0?
        P007_SF2_OK_TOKEN:P007_SF2_FAIL_TOKEN)){
     CloseHandle(com);com=INVALID_HANDLE_VALUE;Sleep(200u);continue;
    }
    if(!send_guest_marker(com,P007_GUEST_READY_TOKEN)){
     CloseHandle(com);com=INVALID_HANDLE_VALUE;Sleep(200u);continue;
    }
   }
   connected=1;
  }
  got=0;
  if(raw_mode){
   if(uart_in((unsigned short)(P007_UART_BASE+5u))&1u){
    byte=uart_in((unsigned short)P007_UART_BASE);
    got=1u;
   }else{
    Sleep(1u);continue;
   }
  }else if(!ReadFile(com,&byte,1u,&got,NULL)){
   CloseHandle(com);com=INVALID_HANDLE_VALUE;connected=0;
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
    if(raw_mode)(void)uart_send_raw(P007_GUEST_STOP_TOKEN);
    else (void)send_guest_marker(com,P007_GUEST_STOP_TOKEN);
    panic(synth);break;
   }
  }
 }
 if(com!=INVALID_HANDLE_VALUE)CloseHandle(com);
 midiOutReset(synth);midiOutClose(synth);
 if(s_creative_handle && s_creative_manager)
   (void)s_creative_manager(s_creative_handle,102u,0,0);
 if(s_creative_sf2_dll)FreeLibrary(s_creative_sf2_dll);
 ExitProcess(3);
}
