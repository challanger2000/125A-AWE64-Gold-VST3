/*
 * P002 independently captured original Creative Win95 WinMM MIDI scenario matrix.
 * Windows 95 / Pentium P54C (i586) / 32-bit PE with KERNEL32+WINMM only.
 * This program sends MIDI; it DOES NOT generate registers or audio itself.
 * The VM's calibrated 86Box EMU8K_TRACE + AWE32_WAV are the independent oracle.
 * Out: C:\P002VEL.LOG on a PRIVATE disposable Win95 guest disk.
 */
#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

static HANDLE logfile=INVALID_HANDLE_VALUE;
static DWORD event_index=0;
static DWORD first_tick=0;
static const char hex[]="0123456789ABCDEF";
static void text(const char*s) {
    DWORD n=0,w=0;
    if(logfile==INVALID_HANDLE_VALUE) return;
    while(s[n])++n;
    if(n && !WriteFile(logfile,s,n,&w,0)) ExitProcess(91);
}
static void word(DWORD x) {
    char s[]="0x00000000";
    int i;
    for(i=0;i<8;++i)s[2+i]=hex[(x>>(28-4*i))&15u];
    text(s);
}
static int has(const char*name,const char*needle){
    unsigned i,j;
    for(i=0;name[i];++i){
        for(j=0;needle[j]&&name[i+j];++j){
            char a=name[i+j],b=needle[j];
            if(a>='a'&&a<='z')a=(char)(a-'a'+'A');
            if(a!=b)break;
        }
        if(!needle[j])return 1;
    }
    return 0;
}
static int original_device(HMIDIOUT *handle) {
    UINT i, matches=0, selected=0, total=midiOutGetNumDevs();
    text("P002_WINMM_DEVICE_COUNT=");word(total);text("\r\n");
    for(i=0;i<total;++i){
        MIDIOUTCAPSA caps;
        DWORD j;
        MMRESULT rc;
        for(j=0;j<sizeof(caps);++j)((char*)&caps)[j]=0;
        rc=midiOutGetDevCapsA(i,&caps,sizeof(caps));
        if(rc!=MMSYSERR_NOERROR){text("DEVICE_ENUM_ERROR=");word(rc);text("\r\n");continue;}
        text("P002_DEVICE ");word(i);text(" NAME=");text(caps.szPname);text("\r\n");
        if(has(caps.szPname,"AWE") && has(caps.szPname,"SYNTH")){
            ++matches;selected=i;
        }
    }
    if(matches!=1){
        text("P002_AMBIGUOUS_OR_MISSING_CREATIVE_DEVICE=");word(matches);text("\r\n");
        return 0;
    }
    {
        MMRESULT rc=midiOutOpen(handle,selected,0,0,CALLBACK_NULL);
        text("P002_CREATIVE_OPEN_RESULT=");word(rc);text("\r\n");
        if(rc!=MMSYSERR_NOERROR)return 0;
    }
    text("P002_ORIGINAL_CREATIVE_WINMM_SELECTED\r\n");
    return 1;
}
static int send(HMIDIOUT device,unsigned status,unsigned d1,unsigned d2,const char *tag) {
    DWORD value,delta;
    MMRESULT rc;
    if(status<0x80||status>=0xF0||d1>127||d2>127)return 0;
    value=(status&255u)|((d1&127u)<<8)|((d2&127u)<<16);
    rc=midiOutShortMsg(device,value);
    delta=GetTickCount()-first_tick;
    text("MIDI seq=");word(event_index++);
    text(" tick_ms=");word(delta);
    text(" name=");text(tag);
    text(" value=");word(value);
    text(" rc=");word(rc);
    text("\r\n");
    return rc==MMSYSERR_NOERROR;
}
#define SEND(s,d1,d2,tag) do {if(!send(dev,s,d1,d2,tag))goto failed;}while(0)
#define WAIT(t) Sleep(t)
static void phase(unsigned id,const char *name){
    text("SCENARIO index=");word(id);
    text(" tick_ms=");word(GetTickCount()-first_tick);
    text(" name=");text(name);
    text("\r\n");
}
void WINAPI p002_entry(void){
    HMIDIOUT dev=0;
    int opened=0;
    first_tick=GetTickCount();
    logfile=CreateFileA("C:\\P002VEL.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,
                        CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    if(logfile==INVALID_HANDLE_VALUE)ExitProcess(29);
    text("P002_ORIGINAL_CREATIVE_WIN95_VELOCITY_SWEEP_START\r\n");
    if(!original_device(&dev))goto failed;
    opened=1;
    for(unsigned program_index=0;program_index<2;program_index++) {
        unsigned v,cc;
        unsigned program=program_index?40u:0u;
        phase(program_index*2,program_index?"PROGRAM40_ALL127_VELOCITIES":"PROGRAM0_ALL127_VELOCITIES");
        SEND(0xB0,121,0,"RESET_CONTROLLERS");
        SEND(0xB0,0,0,"BANK_MSB0");
        SEND(0xB0,32,0,"BANK_LSB0");
        SEND(0xC0,program,0,"PROGRAM_SELECTED");
        SEND(0xB0,7,100,"VOLUME_DEFAULT");
        SEND(0xB0,11,127,"EXPRESSION_DEFAULT");
        SEND(0xB0,10,64,"PAN_CENTER");
        WAIT(140);
        for(v=1;v<=127;v++) {
            SEND(0x90,60,v,"VELOCITY_ON");
            WAIT(40);
            SEND(0x80,60,0,"VELOCITY_OFF");
            WAIT(35);
        }
        WAIT(300);
        phase(program_index*2+1,program_index?"PROGRAM40_CONTROLLER_SWEEP":"PROGRAM0_CONTROLLER_SWEEP");
        SEND(0x90,60,100,"CONTROL_NOTE_ON");
        WAIT(220);
        for(cc=0;cc<=120;cc+=8) {
            SEND(0xB0,7,cc,"CC7_SWEEP");
            WAIT(70);
        }
        SEND(0xB0,7,127,"CC7_MAX");
        WAIT(70);
        SEND(0xB0,7,100,"CC7_RESTORE");
        WAIT(100);
        for(cc=0;cc<=120;cc+=8) {
            SEND(0xB0,11,cc,"CC11_SWEEP");
            WAIT(70);
        }
        SEND(0xB0,11,127,"CC11_MAX");
        WAIT(100);
        for(cc=0;cc<=120;cc+=8) {
            SEND(0xB0,10,cc,"PAN_SWEEP");
            WAIT(70);
        }
        SEND(0xB0,10,127,"PAN_MAX");
        WAIT(70);
        SEND(0xB0,10,64,"PAN_CENTER_RESTORE");
        WAIT(100);
        SEND(0xE0,0,0,"PITCH_MIN");WAIT(80);
        SEND(0xE0,0,64,"PITCH_CENTER");WAIT(80);
        SEND(0xE0,127,127,"PITCH_MAX");WAIT(80);
        SEND(0xE0,0,64,"PITCH_CENTER_RESTORE");WAIT(80);
        SEND(0x80,60,0,"CONTROL_NOTE_OFF");
        WAIT(450);
    }
    text("P002_GUEST_ORIGINAL_VELOCITY_SWEEP_SENT_OK count=");
    word(event_index);text("\r\n");
    midiOutReset(dev);
    midiOutClose(dev);
    FlushFileBuffers(logfile);
    CloseHandle(logfile);
    ExitProcess(0);
failed:
    text("P002_GUEST_ORIGINAL_VELOCITY_SWEEP_FAILED\r\n");
    if(opened){midiOutReset(dev);midiOutClose(dev);}
    FlushFileBuffers(logfile);
    CloseHandle(logfile);
    ExitProcess(19);
}
