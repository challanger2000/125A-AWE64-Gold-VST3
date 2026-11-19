// P007BK: OWNER-PRIVATE ORIGINAL 86Box/Win95/Creative multi-bank sequence.
// Included only inside the existing P007ActualHostQA.cpp anonymous namespace.
// This is a research test host, not a VST3 feature or a synthetic synthesizer.
// Accept ONLY original CPU + in-guest AWEManager ACK + newly completed SF2
// transport generation + actual EMI8000 register writes and nonzero output PCM.
void privateOriginalMultiSf2DrumSequence(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,
    const fs::path& bundle,const fs::path& vintage,
    const fs::path& secondFont) {
    const auto res=bundle/"Contents"/"Resources"/"PrivateVM";
    require(fs::is_regular_file(res/"vm"/"win95.img") &&
            fs::file_size(res/"vm"/"win95.img")==536870912ull &&
            fs::is_directory(res/"roms") && fs::is_directory(res/"assets") &&
            fs::is_regular_file(res/"vm"/"P007_AWE64.cfg") &&
            !fs::exists(res/"P007_SYNTHETIC_TEST_ONLY.marker"),
            "P007BK_NO_PRIVATE_ORIGINAL_WIN95_BUNDLE");
    require(fs::is_regular_file(vintage) &&
            fs::is_regular_file(secondFont),
            "P007BK_TWO_DISTINCT_REAL_SF2_FILES_MISSING");
    require(fs::absolute(vintage)!=fs::absolute(secondFont) &&
            fs::file_size(vintage)!=fs::file_size(secondFont),
            "P007BK_SECOND_SF2_MUST_BE_DIFFERENT_FROM_FIRST");
    const auto progA=sf2ProbeProgram(vintage);
    const auto progB=sf2ProbeProgram(secondFont);
    require(fs::file_size(vintage)>=96u &&
            fs::file_size(vintage)<=28ull*1024ull*1024ull &&
            fs::file_size(secondFont)>=96u &&
            fs::file_size(secondFont)<=28ull*1024ull*1024ull,
            "P007BK_ORIGINAL_CREATIVE_SF2_FILE_SIZE_INVALID");
    std::printf("P007BK_TWO_REAL_SOURCE_SF2_FILES"
                " first=%llu bytes program=%u second=%llu bytes program=%u\n",
       static_cast<unsigned long long>(fs::file_size(vintage)),progA,
       static_cast<unsigned long long>(fs::file_size(secondFont)),progB);
    std::fflush(stdout);

    OwnerGuestSandbox isolated;
    auto plugin=instantiate(factory,id,host);
    auto setProjectState=[&](const std::vector<std::uint8_t>& image) {
        require(!image.empty(),"P007BK_VST3_PROJECT_STATE_ENCODING_INVALID");
        MemoryStream stream;
        int32 wrote=0;
        require(stream.write(
            const_cast<std::uint8_t*>(image.data()),
            static_cast<int32>(image.size()),&wrote)==kResultOk &&
            wrote==static_cast<int32>(image.size()),
            "P007BK_VST3_STATE_WRITE_FAILED");
        rewind(stream);
        require(plugin.component->setState(&stream)==kResultOk,
            "P007BK_VST3_ACTIVE_PROJECT_STATE_RESTORE_FAILED");
    };
    p007::state::Values startRom{};
    startRom.program=static_cast<std::uint8_t>(progA);
    startRom.master=10000u;
    const auto romImage=p007::state::encode(startRom);
    setProjectState(std::vector<std::uint8_t>(
        romImage.begin(),romImage.end()));

    ProcessSetup setup{};
    setup.processMode=kRealtime;
    setup.symbolicSampleSize=kSample32;
    setup.maxSamplesPerBlock=block;
    setup.sampleRate=44100.;
    require(plugin.processor->setupProcessing(setup)==kResultOk,
        "P007BK_ACTUAL_VST3_PROCESS_SETUP_FAILED");
    require(plugin.component->setActive(true)==kResultOk,
        "P007BK_ORIGINAL_WIN95_VST3_ACTIVATE_FAILED");
    bool active=true,processing=false;
    try {
        require(plugin.processor->setProcessing(true)==kResultOk,
            "P007BK_ACTUAL_VST3_PROCESSING_START_FAILED");
        processing=true;
        const HMODULE core=GetModuleHandleW(L"125A_AWE_Core.dll");
        require(core!=nullptr,"P007BK_ORIGINAL_86BOX_CORE_MISSING");
        using IntFn=int (__cdecl*)();
        using CountFn=unsigned (__cdecl*)();
        using SymbolFn=std::uintptr_t (__cdecl*)();
        using ProbeBeginFn=int (__cdecl*)(unsigned);
        using ProbeStopFn=unsigned (__cdecl*)();
        auto getInt=[&](const char* n) {
            return reinterpret_cast<IntFn>(GetProcAddress(core,n));
        };
        auto getCount=[&](const char* n) {
            return reinterpret_cast<CountFn>(GetProcAddress(core,n));
        };
        const auto symbolInit=reinterpret_cast<SymbolFn>(
            GetProcAddress(core,"p007_core_symbol_pc_init"));
        const auto symbolRun=reinterpret_cast<SymbolFn>(
            GetProcAddress(core,"p007_core_symbol_pc_run"));
        const auto ready=getInt("p007_midi_guest_ready");
        const auto creativeAck=getInt("p007_midi_guest_sf2_status");
        const auto transferState=getInt("p007_live_soundfont_state");
        const auto bootCount=getCount("p007_core_guest_cold_boot_count");
        const auto publishCount=getCount("p007_live_soundfont_publish_count");
        const auto completeCount=getCount("p007_live_soundfont_complete_count");
        const auto failCount=getCount("p007_live_soundfont_failure_count");
        const auto probeBegin=reinterpret_cast<ProbeBeginFn>(
            GetProcAddress(core,"p007aj_probe_begin"));
        const auto probeStop=reinterpret_cast<ProbeStopFn>(
            GetProcAddress(core,"p007aj_probe_stop"));
        require(symbolInit && symbolRun &&
                symbolInit()!=0u && symbolRun()!=0u &&
                ready && creativeAck && transferState && bootCount &&
                publishCount && completeCount && failCount &&
                probeBegin && probeStop,
                "P007BK_ORIGINAL_CPU_CREATIVE_TRANSFER_GENERATION_OR_CHIP_PROBE_MISSING");
        std::array<float,block> left{},right{};
        float* channels[2]={left.data(),right.data()};
        AudioBusBuffers out{};
        out.numChannels=2;
        out.channelBuffers32=channels;
        EventList events;
        ProcessData data{};
        data.processMode=kRealtime;
        data.symbolicSampleSize=kSample32;
        data.numSamples=block;
        data.numInputs=0;data.numOutputs=1;
        data.outputs=&out;
        data.inputEvents=&events;
        using Clock=std::chrono::steady_clock;
        auto next=Clock::now();
        auto tick=[&](int action,int pitch,
                      p007::original_audio_proof::Window* capture=nullptr){
            events.clear();
            if(action!=0) {
                Event e{};
                e.type=action>0?Event::kNoteOnEvent:Event::kNoteOffEvent;
                e.busIndex=0;e.sampleOffset=0;
                if(action>0){
                    e.noteOn.channel=0;e.noteOn.pitch=static_cast<int16>(pitch);
                    e.noteOn.velocity=.9f;
                }else{
                    e.noteOff.channel=0;e.noteOff.pitch=static_cast<int16>(pitch);
                    e.noteOff.velocity=0.f;
                }
                require(events.addEvent(e)==kResultOk,
                    "P007BK_HOST_MIDI_EVENT_NOT_ACCEPTED");
            }
            require(plugin.processor->process(data)==kResultOk,
                "P007BK_REAL_VST3_AUDIO_PROCESS_FAILED");
            for(unsigned i=0;i<static_cast<unsigned>(block);++i) {
                const float l=left[i],r=right[i];
                require(std::isfinite(l)&&std::isfinite(r),
                    "P007BK_ORIGINAL_EMU8000_NONFINITE_PCM");
                if(capture)capture->append(l,r);
            }
            next+=std::chrono::microseconds(23220);
            std::this_thread::sleep_until(next);
        };
        const auto started=Clock::now();
        while(!ready() && Clock::now()-started<170s)
            tick(0,0);
        require(ready()!=0 && bootCount()==1u,
            "P007BK_ORIGINAL_WIN95_COLD_BOOT_FAILED");
        require(creativeAck()==0 && publishCount()==0u &&
                completeCount()==0u && failCount()==0u,
            "P007BK_ROM_ONLY_GUEST_MUST_START_WITHOUT_USER_SF2");
        p007::original_audio_proof::Window baseline;
        for(int i=0;i<52;++i)tick(0,0,&baseline);
        require(baseline.frames>=44100u && baseline.rms()<0.0001,
            "P007BK_NO_MIDI_ORIGINAL_ROM_BASELINE_NOT_SILENT");
        std::puts("P007BK_ORIGINAL_WIN95_ROM_ONLY_COLD_BOOT_ONCE_PASS");
        std::fflush(stdout);

        auto installUserBank=[&](const fs::path& source,unsigned program,
                                 const char* marker){
            const auto oldPublish=publishCount();
            const auto oldComplete=completeCount();
            const auto oldFail=failCount();
            p007::state::Values v{};
            v.bank=1u;v.program=static_cast<std::uint8_t>(program);
            v.master=10000u;
            // A true SF2 selection DURING LIVE processing: no guest close.
            const auto encoded=p007::state::encodeWithSf2(
                v,fs::absolute(source).u8string());
            setProjectState(encoded);
            const auto deadline=Clock::now()+100s;
            while(Clock::now()<deadline) {
                tick(0,0);
                if(publishCount()==oldPublish+1u &&
                   completeCount()==oldComplete+1u &&
                   creativeAck()==1 && transferState()==3)
                    break;
                if(failCount()!=oldFail || creativeAck()<0)
                    break;
            }
            std::printf("P007BK_ORIGINAL_CREATIVE_SF2_GENERATION"
              " prev_publish=%u publish=%u prev_complete=%u complete=%u"
              " failed=%u creative_ack=%d live_state=%d cold_boots=%u\n",
              oldPublish,publishCount(),oldComplete,completeCount(),
              failCount(),creativeAck(),transferState(),bootCount());
            std::fflush(stdout);
            require(bootCount()==1u,
                "P007BK_WINDOWS_95_REBOOTED_DURING_LIVE_SF2_REPLACEMENT");
            require(failCount()==oldFail &&
                    publishCount()==oldPublish+1u &&
                    completeCount()==oldComplete+1u &&
                    creativeAck()==1 && transferState()==3,
                "P007BK_TWO_SF2_GENERATIONS_WITH_FRESH_CREATIVE_ACK_NOT_PROVEN");
            std::puts(marker);std::fflush(stdout);
        };
        auto playVoice=[&](const char* label,std::uint8_t bank,
                           unsigned program,int pitch) {
            require(plugin.controller->setParamNormalized(bankId,
                    double(bank)/127.)==kResultOk &&
                    plugin.controller->setParamNormalized(programId,
                    double(program)/127.)==kResultOk,
                    "P007BK_REAL_CREATIVE_MIDI_BANK_PROGRAM_PARAMETER_FAILED");
            // Original CC0, CC32, PC are intentionally paced at >=20ms.
            for(int i=0;i<60;++i)tick(0,0);
            require(probeBegin(unsigned(bank))==1,
                    "P007BK_ORIGINAL_EMU8000_REGISTER_PROBE_START_FAILED");
            p007::original_audio_proof::Window window;
            tick(1,pitch,&window);
            for(int i=0;i<109;++i)tick(0,0,&window);
            const auto chipEvents=probeStop();
            tick(-1,pitch);
            for(int i=0;i<18;++i)tick(0,0);
            std::printf("P007BK_ORIGINAL_PCM_NOTE"
                 " %s bank=%u program=%u note=%d"
                 " pcm_frames=%llu rms=%.9f peak=%.9f rough=%.9f"
                 " emu8k_register_events=%u cold_boots=%u\n",
                label,unsigned(bank),program,pitch,
                static_cast<unsigned long long>(window.frames),
                window.rms(),window.peak,window.roughness(),
                chipEvents,bootCount());
            std::fflush(stdout);
            require(bootCount()==1u &&
                    p007::original_audio_proof::accept(baseline,window) &&
                    chipEvents>0u,
                    "P007BK_ORIGINAL_CREATIVE_MIDI_EMU8000_PCM_NOT_PROVEN");
            return window;
        };
        // Genuine, audibly distinct multi-SF2 hot reloading in ONE guest:
        // owner Vintage Dreams (136 programs), alternate 77-preset bank,
        // then Vintage Dreams again to prove repeatability and fresh ACK.
        installUserBank(vintage,progA,
            "P007BK_VINTAGE_DREAMS_FIRST_HOT_LOAD_ORIGINAL_CREATIVE_ACK_PASS");
        const auto audioA=playVoice("VINTAGE_FIRST",1u,progA,60);
        installUserBank(secondFont,progB,
            "P007BK_DIFFERENT_77_PRESET_SF2_SECOND_HOT_LOAD_ACK_PASS");
        const auto audioB=playVoice("SECOND_USER_SF2",1u,progB,60);
        require(p007::original_audio_proof::distinguishBanks(audioA,audioB),
            "P007BK_TWO_DISTINCT_SF2_FILES_PLAYED_INDISTINGUISHABLE_PCM");
        std::puts("P007BK_TWO_DISTINCT_SF2_GENUINE_PCM_FINGERPRINTS_PASS");
        installUserBank(vintage,progA,
            "P007BK_VINTAGE_DREAMS_THIRD_HOT_LOAD_ACK_PASS");
        const auto again=playVoice("VINTAGE_RETURN",1u,progA,60);
        require(p007::original_audio_proof::distinguishBanks(again,audioB),
            "P007BK_RELOADED_VINTAGE_SOUNDS_LIKE_PRIOR_SF2");
        require(publishCount()==3u && completeCount()==3u &&
                failCount()==0u && bootCount()==1u,
            "P007BK_THREE_LIVE_SF2_LOADS_WERE_NOT_ONE_WIN95_BOOT");
        std::puts("P007BK_THREE_HOT_LOADS_TWO_DIFFERENT_SF2_ONE_WIN95_BOOT_PASS");
        std::fflush(stdout);

        // 127 is the VST3 browser lane for REAL SF2 bank128 drum kits.
        // MIDI program2 = actual Vintage Dreams TR-808 Drumset, not an
        // invented preset. Host notes on ch1 are routed to Creative ch10.
        const auto sf2Drum=playVoice("VINTAGE_TR808_CH10",127u,2u,36);
        std::puts("P007BK_REAL_SF2_BANK128_DRUMKIT_CH10_ORIGINAL_PCM_PASS");
        std::fflush(stdout);
        const auto romMelodic=playVoice("GM_ROM_MELODIC",0u,progA,60);
        require(p007::original_audio_proof::distinguishBanks(audioA,romMelodic),
            "P007BK_ORIGINAL_ROM_SOUND_NOT_DISTINCT_FROM_SF2");
        // Browser lane 126 selects the original Creative GM-ROM drumkit.
        const auto romDrum=playVoice("GM_ROM_STANDARD_CH10",126u,0u,36);
        require(p007::original_audio_proof::distinguishBanks(
                     sf2Drum,romDrum),
                "P007BK_SF2_AND_ROM_DRUM_PCM_INCONCLUSIVE_OR_SAME_KIT");
        std::puts("P007BK_ORIGINAL_ROM_STANDARD_DRUM_CH10_PCM_PASS");
        require(bootCount()==1u && publishCount()==3u &&
                completeCount()==3u && failCount()==0u,
                "P007BK_VM_REBOOT_OR_SF2_TRANSFER_COUNT_REGRESSION");
        std::puts("P007BK_ORIGINAL_CREATIVE_MULTI_SF2_DRUM_AND_ROM_E2E_PASS");
        std::fflush(stdout);
    }catch(...) {
        if(processing)(void)plugin.processor->setProcessing(false);
        if(active)(void)plugin.component->setActive(false);
        throw;
    }
    if(processing)(void)plugin.processor->setProcessing(false);
    if(active)(void)plugin.component->setActive(false);
}
