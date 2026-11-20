/*
 * P002 independently captured original Creative Win95 WinMM MIDI scenario matrix.
 * Windows 95 / Pentium P54C (i586) / 32-bit PE with KERNEL32+WINMM only.
 * This program sends MIDI; it DOES NOT generate registers or audio itself.
 * The VM's calibrated 86Box EMU8K_TRACE + AWE32_WAV are the independent oracle.
 * Out: C:\P002MID.LOG on a PRIVATE disposable Win95 guest disk.
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
    logfile=CreateFileA("C:\\P002MID.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,
                        CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    if(logfile==INVALID_HANDLE_VALUE)ExitProcess(29);
    text("P002_ORIGINAL_CREATIVE_WIN95_MATRIX_START\r\n");
    if(!original_device(&dev))goto failed;
    opened=1;
    phase(0,"RESET_AND_ROM_PROGRAM0");
    SEND(0xB0,121,0,"RESET_ALL_CONTROLLERS");
    SEND(0xC0,0,0,"ROM_PROGRAM_0");
    SEND(0xB0,7,100,"VOLUME_DEFAULT");
    SEND(0xB0,11,127,"EXPRESSION_DEFAULT");
    SEND(0xB0,10,64,"PAN_CENTER");
    WAIT(300);

    phase(1,"VELOCITY_ATTACK_RELEASE");
    SEND(0x90,60,1,"VELOCITY_1_ON");
    WAIT(350);SEND(0x80,60,0,"VELOCITY_1_OFF");
    WAIT(140);
    SEND(0x90,60,64,"VELOCITY_64_ON");
    WAIT(350);SEND(0x80,60,0,"VELOCITY_64_OFF");
    WAIT(140);
    SEND(0x90,60,127,"VELOCITY_127_ON");
    WAIT(350);SEND(0x80,60,0,"VELOCITY_127_OFF");
    WAIT(140);
    SEND(0x90,60,0,"ZERO_VELOCITY_NOTEON_OFF");
    WAIT(300);

    phase(2,"LIVE_CC_PITCH_PRESSURE");
    SEND(0x90,60,96,"HELD_NOTE_ON");WAIT(220);
    SEND(0xB0,7,20,"CC7_20");WAIT(120);
    SEND(0xB0,7,127,"CC7_127");WAIT(120);
    SEND(0xB0,11,30,"CC11_30");WAIT(120);
    SEND(0xB0,11,127,"CC11_127");WAIT(120);
    SEND(0xB0,10,0,"PAN_RIGHT");WAIT(120);
    SEND(0xB0,10,127,"PAN_LEFT");WAIT(120);
    SEND(0xB0,10,64,"PAN_CENTER_RESTORED");WAIT(120);
    SEND(0xB0,91,0,"REVERB_CC91_MIN");WAIT(120);
    SEND(0xB0,91,127,"REVERB_CC91_MAX");WAIT(120);
    SEND(0xB0,93,0,"CHORUS_CC93_MIN");WAIT(120);
    SEND(0xB0,93,127,"CHORUS_CC93_MAX");WAIT(120);
    SEND(0xE0,0,0,"PITCHBEND_MIN");WAIT(150);
    SEND(0xE0,0,64,"PITCHBEND_CENTER");WAIT(150);
    SEND(0xE0,127,127,"PITCHBEND_MAX");WAIT(150);
    SEND(0xE0,0,64,"PITCHBEND_RESTORE");WAIT(120);
    SEND(0xD0,80,0,"CHANNEL_PRESSURE_80");WAIT(120);
    SEND(0xA0,60,100,"POLY_PRESSURE_60_100");WAIT(120);
    SEND(0x80,60,0,"HELD_NOTE_OFF");WAIT(250);

    phase(3,"SUSTAIN_AND_RELEASE");
    SEND(0xB0,64,127,"SUSTAIN_ON");
    SEND(0x90,64,100,"SUSTAINED_NOTE_ON");WAIT(350);
    SEND(0x80,64,0,"SUSTAINED_NOTE_OFF");WAIT(430);
    SEND(0xB0,64,0,"SUSTAIN_OFF");WAIT(350);

    phase(4,"THREE_VOICES_RETRIGGER");
    SEND(0x90,60,100,"CHORD60_ON");WAIT(60);
    SEND(0x90,64,90,"CHORD64_ON");WAIT(60);
    SEND(0x90,67,110,"CHORD67_ON");WAIT(180);
    SEND(0x90,60,70,"RETRIGGER60_ON");WAIT(250);
    SEND(0x80,60,0,"RETRIGGER60_OFF");
    SEND(0x80,64,0,"CHORD64_OFF");
    SEND(0x80,67,0,"CHORD67_OFF");WAIT(450);
    SEND(0xB0,123,0,"ALL_NOTES_OFF");WAIT(180);

    phase(5,"OTHER_CHANNELS_AND_DRUMS");
    SEND(0xC1,40,0,"CHANNEL_1_PROGRAM40");
    SEND(0x91,67,96,"CHANNEL_1_NOTE_ON");WAIT(240);
    SEND(0x81,67,0,"CHANNEL_1_NOTE_OFF");
    SEND(0x99,36,105,"GM_CHANNEL10_KICK_ON");WAIT(180);
    SEND(0x89,36,0,"GM_CHANNEL10_KICK_OFF");
    SEND(0x99,38,105,"GM_CHANNEL10_SNARE_ON");WAIT(180);
    SEND(0x89,38,0,"GM_CHANNEL10_SNARE_OFF");WAIT(300);

    phase(6,"BANK_PROGRAM_CONTROLS_RESET");
    SEND(0xB0,0,0,"BANK_MSB0");
    SEND(0xB0,32,0,"BANK_LSB0");
    SEND(0xC0,40,0,"PROGRAM40");WAIT(160);
    SEND(0x90,72,90,"PROGRAM40_NOTE_ON");WAIT(260);
    SEND(0x80,72,0,"PROGRAM40_NOTE_OFF");WAIT(180);
    SEND(0xB0,120,0,"ALL_SOUND_OFF");WAIT(170);
    SEND(0xB0,121,0,"RESET_CONTROLLERS");WAIT(170);
    SEND(0xB0,123,0,"ALL_NOTES_OFF2");WAIT(170);
    SEND(0xC0,0,0,"PROGRAM0_RESTORE");
    SEND(0xB0,7,100,"CC7_RESTORE");
    SEND(0xB0,11,127,"CC11_RESTORE");
    SEND(0xB0,10,64,"CC10_RESTORE");
    WAIT(350);
    text("P002_GUEST_ORIGINAL_WINMM_MATRIX_SENT_OK count=");
    word(event_index);text("\r\n");
    midiOutReset(dev);
    midiOutClose(dev);
    FlushFileBuffers(logfile);
    CloseHandle(logfile);
    ExitProcess(0);
failed:
    text("P002_GUEST_ORIGINAL_WINMM_MATRIX_FAILED\r\n");
    if(opened){midiOutReset(dev);midiOutClose(dev);}
    FlushFileBuffers(logfile);
    CloseHandle(logfile);
    ExitProcess(19);
}
