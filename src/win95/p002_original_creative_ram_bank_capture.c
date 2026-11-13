/*
 * P002: capture REAL Creative Win95 SoundFont user-bank RAM loading.
 * 1996 Creative WINAWE32.C from the published AWE32 Developer Package:
 * AWEMAN32.DLL / AWEManager; AWE_OPEN=101, AWE_LOAD_USER_BANK=204,
 * AWE_CLOSE=102, user bank #1, CBufferObject OPER_FILE=0.
 * Uses ORIGINAL installed Creative AWEMAN32.DLL & SBAWE.VXD, NOT a
 * replacement software RAM uploader or arbitrary EMU8000 port writes.
 *
 * Separate 86Box calibrated fork EMU8K_TRACE and AWE32_WAV must be
 * captured around this executable. This program writes P002RAM.LOG;
 * successful API return alone DOES NOT prove actual hardware upload.
 * Private C:\P002RAM.SF2 must be staged outside Git along with the
 * original 512MiB Windows95 disk image. No ROM/SF2/VXD is embedded.
 *
 * Source API: qb40/sound-blaster-awe32, WINDOWS/INCLUDE/{AWE_API.H,
 * INDICES.H,AWE_DLL.H}, WINDOWS/SAMPLE/AWEDEMO/WINAWE32.C
 * (original Creative Technology Ltd 1994–1996 developer examples).
 *
 * Pentium P54C Win95 PE32, no C runtime, only kernel32/winmm.
 */
#define WINVER 0x0400
#define _WIN32_WINNT 0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#define ORIGINAL_AWE_OPEN 101u
#define ORIGINAL_AWE_CLOSE 102u
#define ORIGINAL_AWE_LOAD_USER_BANK 204u
#define ORIGINAL_AWE_CLEAR_USER_BANK 205u
#define ORIGINAL_AWE_QUERY_DRAM_SIZE 308u
#define ORIGINAL_AWE_NO_ERR 0L
#define ORIGINAL_OPER_FILE 0u
#define ORIGINAL_USER_BANK_NUMBER 1u

typedef LONG (WINAPI *CREATIVE_AWEMANAGER)(
    DWORD original_handle,UINT original_api,
    LPARAM original_argument1,LPARAM original_argument2);
/* Creative's published INDICES.H CBufferObject Windows32 ABI:
 * DWORD m_Size; LPSTR m_Buffer; DWORD m_SizeUsed; WORD m_Flag;
 * The Win95 API's 32-bit alignment yields sizeof(...) == 16. */
typedef struct original_creative_buffer {
    DWORD m_Size;
    LPSTR m_Buffer;
    DWORD m_SizeUsed;
    WORD m_Flag;
} ORIGINAL_CREATIVE_BUFFER;

static HANDLE log_file=INVALID_HANDLE_VALUE;
static const char hexchars[]="0123456789ABCDEF";
static const char sample_path[]="C:\\P002RAM.SF2";
static const char log_path[]="C:\\P002RAM.LOG";
static DWORD t0=0u;

static void emit(const char*s) {
    DWORD size=0u,written=0u;
    if(log_file==INVALID_HANDLE_VALUE)return;
    while(s[size])++size;
    if(size && (!WriteFile(log_file,s,size,&written,0) ||
                written!=size))ExitProcess(87);
}
static void hex32(DWORD value) {
    char string[]="0x00000000";
    unsigned i;
    for(i=0u;i<8u;++i)
        string[2u+i]=hexchars[(value>>(28u-i*4u))&15u];
    emit(string);
}
static void report(const char*name,DWORD value) {
    emit(name);hex32(value);emit("\r\n");
}
static int contains(const char*hay,const char*needle) {
    unsigned i,j;
    for(i=0u;hay[i];++i) {
        for(j=0u;needle[j]&&hay[i+j];++j) {
            char a=hay[i+j],b=needle[j];
            if(a>='a' && a<='z')a=(char)(a-'a'+'A');
            if(a!=b)break;
        }
        if(!needle[j])return 1;
    }
    return 0;
}
static int original_synth_device(HMIDIOUT*device) {
    const UINT n=midiOutGetNumDevs();
    UINT match=0u,index=0u,i;
    report("WIN95_ORIGINAL_MIDI_ENDPOINTS=",n);
    for(i=0u;i<n;++i) {
        MIDIOUTCAPSA caps;
        MMRESULT rc;
        DWORD j;
        for(j=0u;j<sizeof(caps);++j)
            ((BYTE*)&caps)[j]=0u;
        rc=midiOutGetDevCapsA(i,&caps,sizeof(caps));
        if(rc!=MMSYSERR_NOERROR)continue;
        emit("WIN95_MIDI_DEVICE=");
        hex32(i);emit(" NAME=");emit(caps.szPname);emit("\r\n");
        if(contains(caps.szPname,"AWE") &&
           contains(caps.szPname,"SYNTH")) {
            ++match;index=i;
        }
    }
    report("CREATIVE_ORIGINAL_ENDPOINT_MATCH_COUNT=",match);
    if(match!=1u)return 0;
    {
        const MMRESULT rc=midiOutOpen(device,index,0,0,CALLBACK_NULL);
        report("ORIGINAL_CREATIVE_MIDIOUT_OPEN_RESULT=",rc);
        return rc==MMSYSERR_NOERROR;
    }
}
static int send_midi(HMIDIOUT device,DWORD msg,const char*name) {
    const MMRESULT rc=midiOutShortMsg(device,msg);
    report(name,rc);
    return rc==MMSYSERR_NOERROR;
}
static int original_query_dram(CREATIVE_AWEMANAGER manager,
                               DWORD handle,const char*label,
                               DWORD*total_words,DWORD*available_words) {
    /* Exact original 1994–96 Creative WINAWE32.C query ABI:
       AWEManager(h, AWE_QUERY_DRAM_SIZE, &max, &available).
       Both values are in ORIGINAL EMU8000 16-bit WORDS, not bytes.
       No guessed RAM allocator or post-hoc unit conversion. */
    DWORD total=0u,available=0u;
    const LONG rc=manager(handle,ORIGINAL_AWE_QUERY_DRAM_SIZE,
                          (LPARAM)&total,(LPARAM)&available);
    emit(label);emit("_RESULT=");hex32((DWORD)rc);emit("\r\n");
    if(rc!=ORIGINAL_AWE_NO_ERR)return 0;
    emit(label);emit("_TOTAL_16BIT_WORDS=");hex32(total);emit("\r\n");
    emit(label);emit("_FREE_16BIT_WORDS=");hex32(available);emit("\r\n");
    *total_words=total;
    *available_words=available;
    return 1;
}
static int sample_identity(DWORD*byte_count) {
    HANDLE f;
    DWORD result=0u;
    BYTE head[12];
    DWORD read=0u;
    f=CreateFileA(sample_path,GENERIC_READ,FILE_SHARE_READ,
                  0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(f==INVALID_HANDLE_VALUE)return 0;
    result=GetFileSize(f,0);
    if(result<96u || result>32u*1024u*1024u ||
       !ReadFile(f,head,12u,&read,0) || read!=12u ||
       head[0]!='R'||head[1]!='I'||head[2]!='F'||head[3]!='F'||
       head[8]!='s'||head[9]!='f'||head[10]!='b'||head[11]!='k') {
        CloseHandle(f);return 0;
    }
    CloseHandle(f);
    *byte_count=result;
    return 1;
}
void WINAPI p002_ram_entry(void) {
    HMODULE dll=0;
    CREATIVE_AWEMANAGER manager=0;
    DWORD handle=0u,sample_bytes=0u;
    DWORD max_before=0u,free_before=0u;
    DWORD max_after=0u,free_after=0u;
    LONG rc;
    HMIDIOUT midi=0;
    ORIGINAL_CREATIVE_BUFFER buffer;
    char userbank_file_buffer[128]="C:\\P002RAM.SF2";
    char dllpath[260];
    DWORD p=0u;
    int opened=0,loaded=0,haveMidi=0,success=0;
    log_file=CreateFileA(log_path,GENERIC_WRITE,FILE_SHARE_READ,0,
                         CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    if(log_file==INVALID_HANDLE_VALUE)ExitProcess(29);
    t0=GetTickCount();
    emit("P002_WIN95_ORIGINAL_CREATIVE_RAM_BANK_CAPTURE_START\r\n");
    if(sizeof(ORIGINAL_CREATIVE_BUFFER)!=16u) {
        emit("ORIGINAL_AWEMAN32_CBUFFER_ABI_UNEXPECTED\r\n");
        goto done;
    }
    if(!sample_identity(&sample_bytes)) {
        emit("PRIVATE_SAMPLE_MISSING_OR_NOT_SFBK\r\n");
        goto done;
    }
    report("PRIVATE_WIN95_SF2_SIZE=",sample_bytes);
    /* Do not silently load AWEMAN.DLL fallback, OpenAL or non-original
       Windows synthesizer. Win95 installed Creative AWEMAN32 mandatory. */
    dll=LoadLibraryA("AWEMAN32.DLL");
    if(!dll) {
        report("ORIGINAL_AWEMAN32_LOAD_ERROR=",GetLastError());
        goto done;
    }
    for(p=0u;p<sizeof(dllpath);++p)dllpath[p]=0;
    if(GetModuleFileNameA(dll,dllpath,sizeof(dllpath))==0u) {
        emit("ORIGINAL_AWEMAN32_PATH_UNKNOWN\r\n");goto done;
    }
    emit("LOADED_CREATIVE_MODULE_PATH=");emit(dllpath);emit("\r\n");
    manager=(CREATIVE_AWEMANAGER)
        GetProcAddress(dll,"AWEManager");
    if(!manager) {
        emit("ORIGINAL_AWEMAN32_EXPORT_AWEMANAGER_MISSING\r\n");goto done;
    }
    rc=manager(0u,ORIGINAL_AWE_OPEN,(LPARAM)&handle,0);
    report("ORIGINAL_AWEMAN32_AWE_OPEN_RC=",(DWORD)rc);
    if(rc!=ORIGINAL_AWE_NO_ERR || handle==0u)goto done;
    opened=1;
    if(!original_synth_device(&midi))goto done;
    haveMidi=1;
    /* Original source API 308: inspect driver-reported DRAM before
       letting Creative allocate/upload the private SoundFont. */
    if(!original_query_dram(manager,handle,"PRE_LOAD",
                            &max_before,&free_before))
        goto done;
    if(max_before==0u || free_before>max_before) {
        emit("ORIGINAL_DRIVER_PRELOAD_RAM_QUERY_UNUSABLE\r\n");goto done;
    }
    /* Exact original 1996 Creative example CBufferObject and
       AWEManager(user, AWE_LOAD_USER_BANK, USER_BANK_NUM, &buffer).
       No sample placement, conversion or RAM allocation occurs here;
       ALL of it is performed by the original installed Creative driver. */
    /* The original Creative WINAWE32 sample uses a *mutable 128-byte*
       filename buffer and m_Size=128, not a string literal pointer. */
    buffer.m_Size=sizeof(userbank_file_buffer);
    buffer.m_Buffer=userbank_file_buffer;
    buffer.m_SizeUsed=0u;
    buffer.m_Flag=ORIGINAL_OPER_FILE;
    report("ORIGINAL_API_LOAD_BEGIN_TICK_MS=",GetTickCount()-t0);
    emit("ORIGINAL_WIN95_RAM_PORT_CAPTURE_MARKER_BEGIN\r\n");
    rc=manager(handle,ORIGINAL_AWE_LOAD_USER_BANK,
               (LPARAM)ORIGINAL_USER_BANK_NUMBER,(LPARAM)&buffer);
    report("ORIGINAL_AWEMAN32_LOAD_USER_BANK_RC=",(DWORD)rc);
    report("ORIGINAL_API_LOAD_END_TICK_MS=",GetTickCount()-t0);
    emit("ORIGINAL_WIN95_RAM_PORT_CAPTURE_MARKER_END\r\n");
    if(rc!=ORIGINAL_AWE_NO_ERR)goto done;
    loaded=1;
    if(!original_query_dram(manager,handle,"POST_LOAD",
                            &max_after,&free_after))
        goto done;
    if(max_after!=max_before || free_after>free_before) {
        emit("ORIGINAL_DRIVER_DRAM_SIZE_INCONSISTENT\r\n");
        goto done;
    }
    report("CREATIVE_DRIVER_ALLOCATED_WORD_DELTA=",
           free_before-free_after);
    emit("GUEST_DRAM_ALLOCATION_API_ALONE_NOT_HARDWARE_UPLOAD_PROOF\r\n");
    /* The bank-load phase above is the authoritative DRAM-upload
       measurement. The following MIDI-only phase is separate and
       cannot prove selection of a specific bank without register/PCM
       evidence. Bank Select 1 is a controlled PROBE, not a claimed
       original driver program bank mapping formula. */
    Sleep(200);
    if(!send_midi(midi,0x000079B0u,"ORIGINAL_RESET_CONTROLLERS_RC"))goto done;
    if(!send_midi(midi,0x000000B0u | (1u<<8),
                  "ORIGINAL_BANK_SELECT_MSB1_RC"))goto done;
    if(!send_midi(midi,0x000000C0u,"ORIGINAL_PROGRAM0_RC"))goto done;
    /* DRY comparison first. Original MIDI controllers 91/93 = 0:
       zero requested reverb/chorus send, not a claim that Creative's
       master/mixer FX or unverified DRY PCM are automatically identical. */
    if(!send_midi(midi,0x00005BB0u,"ORIGINAL_CC91_REVERB_SEND_ZERO_RC"))goto done;
    if(!send_midi(midi,0x00005DB0u,"ORIGINAL_CC93_CHORUS_SEND_ZERO_RC"))goto done;
    Sleep(150);
    if(!send_midi(midi,0x00643C90u,"ORIGINAL_MIDI_C4_VEL100_RC"))goto done;
    Sleep(400);
    if(!send_midi(midi,0x00003C80u,"ORIGINAL_MIDI_C4_OFF_RC"))goto done;
    Sleep(500);
    success=1;
done:
    if(haveMidi) {
        midiOutReset(midi);
        midiOutClose(midi);
    }
    if(loaded) {
        rc=manager(handle,ORIGINAL_AWE_CLEAR_USER_BANK,
                   (LPARAM)ORIGINAL_USER_BANK_NUMBER,0);
        report("ORIGINAL_AWEMAN32_CLEAR_USER_BANK_RC=",(DWORD)rc);
    }
    if(opened) {
        rc=manager(handle,ORIGINAL_AWE_CLOSE,0,0);
        report("ORIGINAL_AWEMAN32_AWE_CLOSE_RC=",(DWORD)rc);
    }
    if(dll)FreeLibrary(dll);
    emit(success?"P002_WIN95_ORIGINAL_CREATIVE_RAM_LOAD_API_SENT_OK\r\n":
                 "P002_WIN95_ORIGINAL_CREATIVE_RAM_LOAD_FAILED\r\n");
    emit("GUEST_CAPTURE_EMU8000_RAM_REGISTER_AND_PCM_EVIDENCE_NOT_ASSERTED_BY_API\r\n");
    FlushFileBuffers(log_file);
    CloseHandle(log_file);
    ExitProcess(success?0u:19u);
}
