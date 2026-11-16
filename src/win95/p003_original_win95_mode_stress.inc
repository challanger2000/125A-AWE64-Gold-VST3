/* P003 original-Win95 Creative voice-mode coverage stimulus.
   Included ONLY in separately compiled -DP003_ORIGINAL_MODE_STRESS=1
   Windows95 guest test; original P002RAM baseline unaffected.

   All MIDI bytes are passed to ORIGINAL installed AWEMAN32 AWEManager
   API 401, never to candidate native voice allocator or fake synth.
   This does not GUARANTEE mode1/mode2 selection: genuine original
   SF2 descriptor flags and device voice state choose the x86 branch.
   A dedicated C:\P003MODE.LOG records submission, not audio parity.
*/
static HANDLE p003_mode_file=INVALID_HANDLE_VALUE;
static DWORD p003_mode_sent=0u,p003_mode_error=0u,p003_mode_banks_done=0u;
static void p003_write(const char *s){
    DWORD len=0u,out=0u;
    if(p003_mode_file==INVALID_HANDLE_VALUE)return;
    while(s[len])++len;
    if(len && (!WriteFile(p003_mode_file,s,len,&out,0)||out!=len))
        ExitProcess(89);
}
static void p003_number(const char *key,DWORD value){
    char str[]="0x00000000";
    unsigned i;
    for(i=0u;i<8u;++i)str[i+2u]=hexchars[(value>>(28u-4u*i))&15u];
    p003_write(key);p003_write("=");p003_write(str);p003_write("\r\n");
}
static DWORD p003_packet(DWORD ch,DWORD status,DWORD d1,DWORD d2){
    return (status|(ch&15u))|((d1&127u)<<8u)|((d2&127u)<<16u);
}
static int p003_original_midi(CREATIVE_AWEMANAGER mgr,DWORD h,DWORD packet){
    const LONG result=mgr(h,ORIGINAL_AWE_SEND_MIDI,(LPARAM)packet,0);
    ++p003_mode_sent;
    if(result!=ORIGINAL_AWE_NO_ERR){
        ++p003_mode_error;
        if(p003_mode_error==1u)
            p003_number("FIRST_ORIGINAL_CREATIVE_ERROR",(DWORD)result);
        return 0;
    }
    return 1;
}
static int p003_stress_bank(CREATIVE_AWEMANAGER mgr,DWORD h,DWORD bank){
    static const BYTE programs[]={
        0u,7u,10u,16u,24u,32u,40u,48u,
        52u,56u,64u,72u,80u,88u,96u,127u};
    static const BYTE notes[]={
        36u,40u,43u,48u,52u,55u,60u,64u,67u,72u,76u,79u};
    static const BYTE channels[]={0u,2u,9u};
    DWORD p,c,n;
    const DWORD begin=p003_mode_sent;
    p003_number("BANK_PHASE_BEGIN",bank);
    for(p=0u;p<sizeof(programs);++p){
        for(c=0u;c<sizeof(channels);++c){
            const DWORD ch=channels[c];
            if(!p003_original_midi(mgr,h,p003_packet(ch,0xb0u,0u,bank)) ||
               !p003_original_midi(mgr,h,p003_packet(ch,0xc0u,programs[p],0u)) ||
               !p003_original_midi(mgr,h,p003_packet(ch,0xb0u,91u,0u)) ||
               !p003_original_midi(mgr,h,p003_packet(ch,0xb0u,93u,0u)))
                return 0;
        }
        /* 36 overlapping notes across 3 MIDI channels, all NoteOn
           before the first NoteOff. Challenge genuine 32-voice VXD
           allocator and original voice-stealing logic. */
        for(c=0u;c<sizeof(channels);++c)
            for(n=0u;n<sizeof(notes);++n)
                if(!p003_original_midi(mgr,h,p003_packet(
                        channels[c],0x90u,notes[n],
                        ((p+n+c)&1u)?112u:72u)))
                    return 0;
        Sleep(90);
        for(c=0u;c<sizeof(channels);++c){
            for(n=sizeof(notes);n>0u;--n)
                if(!p003_original_midi(mgr,h,p003_packet(
                        channels[c],0x80u,notes[n-1u],0u)))
                    return 0;
            if(!p003_original_midi(mgr,h,p003_packet(
                    channels[c],0xb0u,123u,0u)))
                return 0;
        }
        Sleep(45);
    }
    p003_number(bank==0u?"BANK0_ORIGINAL_CREATIVE_MIDI_CALLS":
                bank==1u?"BANK1_ORIGINAL_CREATIVE_MIDI_CALLS":
                         "BANK2_ORIGINAL_CREATIVE_MIDI_CALLS",
                p003_mode_sent-begin);
    p003_number("BANK_PHASE_END",bank);
    ++p003_mode_banks_done;
    return 1;
}
static int p003_start_stress(void){
    p003_mode_file=CreateFileA("C:\\P003MODE.LOG",GENERIC_WRITE,
        FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    if(p003_mode_file==INVALID_HANDLE_VALUE)return 0;
    p003_write("P003_ORIGINAL_WIN95_ALLOCATOR_MODE_STRESS_V1\r\n");
    p003_number("AWE_SEND_MIDI_OPCODE",ORIGINAL_AWE_SEND_MIDI);
    p003_number("PROGRAMS_PER_BANK",16u);
    p003_number("CHANNELS_PER_PROGRAM",3u);
    p003_number("OVERLAPPING_NOTES_PER_PROGRAM",36u);
    p003_number("GUEST_START_TICK_MS",GetTickCount()-t0);
    return 1;
}
static void p003_stop_stress(int complete){
    if(p003_mode_file!=INVALID_HANDLE_VALUE){
        p003_number("ORIGINAL_MIDI_CALLS_TOTAL",p003_mode_sent);
        p003_number("ORIGINAL_MIDI_CALL_ERRORS",p003_mode_error);
        p003_number("ORIGINAL_BANK_PHASES_COMPLETED",p003_mode_banks_done);
        p003_number("GUEST_END_TICK_MS",GetTickCount()-t0);
        p003_write(complete?
            "P003_ORIGINAL_WIN95_ALLOCATOR_STRESS_COMPLETED\r\n":
            "P003_ORIGINAL_WIN95_ALLOCATOR_STRESS_NO_GO\r\n");
        FlushFileBuffers(p003_mode_file);
        CloseHandle(p003_mode_file);
        p003_mode_file=INVALID_HANDLE_VALUE;
    }
}
