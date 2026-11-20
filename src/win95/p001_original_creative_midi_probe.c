/*
 * P001: private Windows 95 original Creative WinMM probe.
 * Uses ONLY KERNEL32.DLL + WINMM.DLL; no C runtime, no Windows 11 APIs.
 * Based on the verified legacy P63/P66 agent design, but no COM1 dependency.
 * Sends deterministic actual NoteOn/Off to ORIGINAL "AWE32 MIDI Synth".
 * It does not synthesize audio. 86Box EMU8K_TRACE and AWE32_WAV are the oracle.
 */
#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

static HANDLE output=INVALID_HANDLE_VALUE;
static void emit(const char *s) {
    DWORD len=0,written=0;
    if(output==INVALID_HANDLE_VALUE) return;
    while(s[len]) ++len;
    if(len && !WriteFile(output,s,len,&written,0)) return;
    FlushFileBuffers(output);
}
static void hex32(DWORD x) {
    static const char digits[]="0123456789ABCDEF";
    char str[]="0x00000000";
    unsigned i;
    for(i=0;i<8;i++) str[2+i]=digits[(x>>(28-4*i))&15u];
    emit(str);
}
static char upper(char c) {
    return c>='a' && c<='z' ? (char)(c - 'a' + 'A') : c;
}
static int contains(const char *a,const char *b) {
    int i,j;
    for(i=0;a[i];i++) {
        for(j=0;b[j] && a[i+j] && upper(a[i+j])==b[j];j++) {}
        if(!b[j]) return 1;
    }
    return 0;
}
static int send(HMIDIOUT dev, DWORD value, const char* label) {
    MMRESULT res=midiOutShortMsg(dev,value);
    emit(label);emit(" value=");hex32(value);
    emit(" result=");hex32(res);emit("\r\n");
    return res==MMSYSERR_NOERROR;
}
static int open_original(HMIDIOUT* dev) {
    UINT count=midiOutGetNumDevs(),i;
    int found=0;
    emit("WINMM device count=");hex32(count);emit("\r\n");
    for(i=0;i<count;i++) {
        MIDIOUTCAPSA caps;
        MMRESULT res;
        DWORD j;
        for(j=0;j<sizeof(caps);j++) ((char*)&caps)[j]=0;
        res=midiOutGetDevCapsA(i,&caps,sizeof(caps));
        if(res!=MMSYSERR_NOERROR) {
            emit("MIDIOUTCAPS ERROR=");hex32(res);emit("\r\n");
            continue;
        }
        emit("MIDI_DEVICE ");hex32(i);emit(" name=");emit(caps.szPname);emit("\r\n");
        if(!contains(caps.szPname,"AWE") || !contains(caps.szPname,"SYNTH"))
            continue; /* specifically NOT MPU401/MIDI Mapper/SB16 FM synth */
        if(found) {
            emit("ERROR multiple AWE synth endpoints; refusing ambiguity\r\n");
            return 0;
        }
        found=1;
        res=midiOutOpen(dev,i,0,0,CALLBACK_NULL);
        emit("AWE MIDI OPEN result=");hex32(res);emit("\r\n");
        if(res!=MMSYSERR_NOERROR)return 0;
        emit("ORIGINAL_CREATIVE_AWE_WINMM_SELECTED\r\n");
        return 1;
    }
    emit("ERROR original AWE synthesizer not available\r\n");
    return 0;
}
static int note(HMIDIOUT dev,unsigned pitch,unsigned vel) {
    /* status 0x90; data1 pitch; data2 velocity. Strict no fallback. */
    DWORD on=0x90u|((pitch&127u)<<8)|((vel&127u)<<16);
    DWORD off=0x80u|((pitch&127u)<<8);
    if(!send(dev,on,"NOTE_ON"))return 0;
    Sleep(630);
    if(!send(dev,off,"NOTE_OFF"))return 0;
    Sleep(180);
    return 1;
}
void WINAPI p001_entry(void) {
    HMIDIOUT dev=0;
    int ok=0;
    output=CreateFileA("C:\\P001MID.LOG",GENERIC_WRITE,FILE_SHARE_READ,
                       0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    emit("P001 ORIGINAL CREATIVE WIN95 MIDI PROBE ENTRYPOINT\r\n");
    if(output==INVALID_HANDLE_VALUE)ExitProcess(29);
    if(!open_original(&dev))goto done;
    Sleep(250);
    if(!send(dev,0x000000B0u | (121u<<8),"RESET_ALL_CONTROLLERS"))goto closed;
    if(!send(dev,0x000000C0u,"PROGRAM_0_ROM_GM"))goto closed;
    if(!send(dev,0x007F07B0u,"CC7_VOLUME"))goto closed;
    if(!send(dev,0x007F0BB0u,"CC11_EXPRESSION"))goto closed;
    Sleep(180);
    if(!note(dev,48u,100u))goto closed;
    if(!note(dev,60u,96u))goto closed;
    if(!note(dev,64u,96u))goto closed;
    if(!note(dev,67u,96u))goto closed;
    if(!note(dev,72u,108u))goto closed;
    Sleep(300);
    ok=1;
closed:
    midiOutReset(dev);
    midiOutClose(dev);
done:
    emit(ok?"P001_GUEST_WINMM_MESSAGES_SENT_OK\r\n":
            "P001_GUEST_WINMM_MESSAGES_FAILED\r\n");
    if(output!=INVALID_HANDLE_VALUE) {
        FlushFileBuffers(output);
        CloseHandle(output);
    }
    ExitProcess(ok?0:19);
}
