/* Owner-private Win95 Creative idle probe, 1996 public AWE API IDs.
   The ORIGINAL installed AWEMAN32.DLL loads the owner's SoundFont.
   Deliberately sends NO MIDI NoteOn/Off and never patches Creative binaries.
   PE32 i586 Windows 95, no CRT, KERNEL32 only. */
#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#define AWE_OPEN 101u
#define AWE_CLOSE 102u
#define AWE_LOAD_USER_BANK 204u
#define AWE_CLEAR_USER_BANK 205u
typedef LONG (WINAPI *CREATIVE_MANAGER)(DWORD,UINT,LPARAM,LPARAM);
#pragma pack(push,2)
typedef struct { DWORD size; LPSTR buffer; DWORD size_used; WORD flag; } BUFFER;
#pragma pack(pop)
typedef char creative_1996_cbuffer_ABI[(sizeof(BUFFER)==14u &&
    offsetof(BUFFER,flag)==12u)?1:-1];
static HANDLE log_handle=INVALID_HANDLE_VALUE;
static void say(const char* p) {
    DWORD n=0u,w=0u;
    if(log_handle==INVALID_HANDLE_VALUE)return;
    while(p[n])++n;
    if(n && (!WriteFile(log_handle,p,n,&w,0)||w!=n))ExitProcess(87);
    FlushFileBuffers(log_handle);
}
static void hex32(DWORD v) {
    static const char digits[]="0123456789ABCDEF";
    char s[]="0x00000000";
    unsigned i;
    for(i=0;i<8u;++i)s[2u+i]=digits[(v>>(28u-4u*i))&15u];
    say(s);
}
static void result(const char* n,DWORD v){say(n);hex32(v);say("\r\n");}
void WINAPI p002_ram_entry(void) {
    HMODULE dll=0;
    CREATIVE_MANAGER manager=0;
    DWORD handle=0;
    LONG rc=1;
    int opened=0,loaded=0;
    char filename[128]="C:\\P002RAM.SF2";
    BUFFER buffer;
    log_handle=CreateFileA("C:\\P002IDLE.LOG",GENERIC_WRITE,
        FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    if(log_handle==INVALID_HANDLE_VALUE)ExitProcess(29);
    say("P002_ORIGINAL_CREATIVE_IDLE_SOURCE_PROBE_START\r\n");
    if(GetFileAttributesA(filename)==INVALID_FILE_ATTRIBUTES){
        say("P002_PRIVATE_SOUND_FONT_MISSING\r\n");goto done;
    }
    dll=LoadLibraryA("AWEMAN32.DLL");
    if(!dll){result("P002_AWEMAN32_LOAD_ERROR=",GetLastError());goto done;}
    manager=(CREATIVE_MANAGER)GetProcAddress(dll,"AWEManager");
    if(!manager){say("P002_ORIGINAL_AWEMANAGER_MISSING\r\n");goto done;}
    rc=manager(0u,AWE_OPEN,(LPARAM)&handle,0);
    result("P002_ORIGINAL_AWE_OPEN_RC=",(DWORD)rc);
    if(rc!=0 || handle==0u)goto done;
    opened=1;
    buffer.size=sizeof(filename);
    buffer.buffer=filename;
    buffer.size_used=0u;
    buffer.flag=0u; /* ORIGINAL_OPER_FILE */
    rc=manager(handle,AWE_LOAD_USER_BANK,(LPARAM)1u,(LPARAM)&buffer);
    result("P002_ORIGINAL_LOAD_USER_BANK_RC=",(DWORD)rc);
    if(rc!=0)goto done;
    loaded=1;
    say("P002_ORIGINAL_CREATIVE_IDLE_READY_NO_MIDI\r\n");
    Sleep(180000u);
done:
    say("P002_ORIGINAL_CREATIVE_IDLE_GUEST_EXIT\r\n");
    if(loaded){
        rc=manager(handle,AWE_CLEAR_USER_BANK,(LPARAM)1u,0);
        result("P002_ORIGINAL_CLEAR_USER_BANK_RC=",(DWORD)rc);
    }
    if(opened){
        rc=manager(handle,AWE_CLOSE,0,0);
        result("P002_ORIGINAL_AWE_CLOSE_RC=",(DWORD)rc);
    }
    if(dll)FreeLibrary(dll);
    CloseHandle(log_handle);
    ExitProcess(loaded?0u:19u);
}
