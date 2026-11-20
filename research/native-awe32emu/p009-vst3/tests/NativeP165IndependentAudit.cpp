// Independent audit of the ACTUAL exported VST3. Production sources are
// unchanged. Reuse only the existing SDK host/lifecycle scaffolding.
// Audio cases require explicitly generated SYNTHETIC ROM/SF2 fixtures.
#define main ExistingNativeHostAcceptanceMain
#include "P010NativeHostQA.cpp"
#undef main

namespace {
unsigned failures=0;
void audit(bool okay,const char* name) {
    std::printf("AUDIT_%s=%s\n",name,okay?"PASS":"FAIL");
    if(!okay)++failures;
}
std::vector<std::uint8_t> auditSave(Plugin& p) {
    MemoryStream out;
    check(p.component->getState(&out)==kResultOk,"audit save fixture");
    const auto* b=static_cast<const std::uint8_t*>(out.getData());
    return {b,b+out.getSize()};
}
void auditRestore(Plugin& p,std::vector<std::uint8_t> const& bytes) {
    MemoryStream in;int32 written=0;
    check(in.write(const_cast<std::uint8_t*>(bytes.data()),
        static_cast<int32>(bytes.size()),&written)==kResultOk &&
        written==static_cast<int32>(bytes.size()),"audit write fixture");
    check(in.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk &&
        p.component->setState(&in)==kResultOk,"audit restore fixture");
}
void auditV5(const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host) {
    Plugin original(factory,id,host);
    auto bytes=auditSave(original);
    check(bytes.size()==580,"audit V4 initial fixture size");
    Saved target{};std::memcpy(&target,bytes.data(),580);
    target.advancedStates[2]=0;target.advancedStates[3]=0;
    target.advancedStates[4]=12;target.parameterModes[0]=1;
    target.advancedStates[8*8]=1;target.advancedStates[8*8+1]=0;
    std::memcpy(bytes.data(),&target,580);
    auditRestore(original,bytes);
    audit(auditSave(original)==bytes,"V4_RPN_BANK_POSITIVE_CONTROL");
    // A real user simply edits AMP Attack before saving the same song.
    check(original.controller->setParamNormalized(1100u,.5)==kResultOk,
          "audit real GUI parameter edit");
    auto v5=auditSave(original);
    check(v5.size()==648 && v5[4]==5,"audit V5 extension precondition");
    Plugin reopened(factory,id,host);auditRestore(reopened,v5);
    const auto recalled=auditSave(reopened);
    check(recalled.size()==648,"audit recalled V5 size");
    Saved actual{};std::memcpy(&actual,recalled.data(),580);
    std::printf("AUDIT_V5_RPN_DATA expected=12 actual=%u mode_expected=1 mode_actual=%u\n",
        unsigned(actual.advancedStates[4]),unsigned(actual.parameterModes[0]));
    audit(recalled==v5,"V5_GUI_PLUS_RPN_BANK_RECALL");
}
void auditProgramMapping(const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host) {
    Plugin p(factory,id,host);IMidiMapping* raw=nullptr;
    check(p.controller->queryInterface(IMidiMapping::iid,
        reinterpret_cast<void**>(&raw))==kResultOk && raw,"audit MIDI interface");
    auto mapping=owned(raw);unsigned programs=0,volumes=0;
    for(int16 channel=0;channel<16;++channel) {
        ParamID parameter=0;
        volumes+=mapping->getMidiControllerAssignment(0,channel,kCtrlVolume,parameter)==kResultTrue;
        programs+=mapping->getMidiControllerAssignment(0,channel,kCtrlProgramChange,parameter)==kResultTrue;
    }
    audit(volumes==16,"CC7_MAPPING_POSITIVE_CONTROL");
    std::printf("AUDIT_PROGRAM_CHANGE_MAPPING expected=16 actual=%u\n",programs);
    audit(programs==16,"STANDARD_MIDI_PROGRAM_CHANGE");
}
void auditFlush(const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host) {
    Plugin p(factory,id,host);p.start(0,0,44100.);
    ParameterChanges points(1);int32 qi=0,pi=0;
    auto* queue=points.addParameterData(1000u,qi);
    check(queue && queue->addPoint(0,7./127.,pi)==kResultOk,"audit flush point");
    ProcessData data{};data.symbolicSampleSize=kSample32;data.numSamples=0;
    data.inputParameterChanges=&points;
    const auto result=p.processor->process(data);
    auto bytes=auditSave(p);Saved saved{};std::memcpy(&saved,bytes.data(),580);
    std::printf("AUDIT_ZERO_FRAME_PROGRAM expected=7 actual=%u result=%d\n",saved.program,result);
    audit(result==kResultOk && saved.program==7,"ZERO_FRAME_PARAMETER_FLUSH");
}
std::array<float,2048> auditMasterCapture(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,int pointOffset) {
    Plugin p(factory,id,host);p.start(0,0,44100.);
    std::array<float,1024> left{},right{};
    float* channels[]{left.data(),right.data()};
    AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=channels;
    EventList events;Event on{};on.type=Event::kNoteOnEvent;
    on.noteOn.channel=0;on.noteOn.pitch=60;on.noteOn.velocity=1.f;
    check(events.addEvent(on)==kResultOk,"audit synthetic sample NoteOn");
    ProcessData data{};data.symbolicSampleSize=kSample32;data.processMode=kRealtime;
    data.numSamples=128;data.numOutputs=1;data.outputs=&bus;data.inputEvents=&events;
    check(p.processor->process(data)==kResultOk,"audit actual VST3 warm-up");
    events.clear();ParameterChanges changes(1);
    if(pointOffset>=0) {
        int32 qi=0,pi=0;auto* q=changes.addParameterData(1001u,qi);
        check(q && q->addPoint(pointOffset,0.,pi)==kResultOk,"audit Master point");
    }
    data.numSamples=1024;data.inputParameterChanges=&changes;
    check(p.processor->process(data)==kResultOk,"audit actual VST3 Master block");
    std::array<float,2048> pcm{};
    for(unsigned i=0;i<1024;++i){pcm[2*i]=left[i];pcm[2*i+1]=right[i];}
    return pcm;
}
void auditMaster(const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host) {
    const auto baseline=auditMasterCapture(factory,id,host,-1);
    const auto muteAtStart=auditMasterCapture(factory,id,host,0);
    const auto muteAt512=auditMasterCapture(factory,id,host,512);
    double baselineEnergy=0,earlyEnergy=0;
    for(unsigned i=0;i<1024;++i) {
        baselineEnergy+=double(baseline[i])*baseline[i];
        earlyEnergy+=double(muteAt512[i])*muteAt512[i];
    }
    check(baselineEnergy>0,"audit synthetic actual-chip prefix must be audible");
    audit(std::all_of(muteAtStart.begin(),muteAtStart.end(),[](float x){return x==0;}),
          "MASTER_FRAME_ZERO_MUTE_POSITIVE_CONTROL");
    const bool samePrefix=std::equal(baseline.begin(),baseline.begin()+1024,muteAt512.begin());
    std::printf("AUDIT_MASTER_512 prefix_energy_expected=%.9g actual=%.9g same_prefix=%u\n",
        baselineEnergy,earlyEnergy,unsigned(samePrefix));
    audit(samePrefix,"MASTER_SAMPLE_OFFSET_PRESERVES_EARLIER_AUDIO");
    audit(std::all_of(muteAt512.begin()+1024,muteAt512.end(),[](float x){return x==0;}),
          "MASTER_AFTER_512_IS_MUTED");
}
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"Usage: NativeP165IndependentAudit <synthetic-test-VST3-bundle>");
        std::string error;auto module=VST3::Hosting::Module::create(argv[1],error);
        check(bool(module),"audit loads actual Windows VST3");
        auto host=owned(new HostApplication());const auto& factory=module->getFactory();
        const auto classes=factory.classInfos();
        const auto cls=std::find_if(classes.begin(),classes.end(),
            [](auto const& c){return c.category()==kVstAudioEffectClass;});
        check(cls!=classes.end(),"audit instrument class");
        auditV5(factory,cls->ID(),host.get());
        auditProgramMapping(factory,cls->ID(),host.get());
        auditFlush(factory,cls->ID(),host.get());
        auditMaster(factory,cls->ID(),host.get());
        std::printf("AUDIT_CONFIRMED_CONTRACT_FAILURES=%u synthetic_audio_only=1\n",failures);
        return failures?1:0;
    } catch(std::exception const& e) {
        std::fprintf(stderr,"AUDIT_FIXTURE_OR_RUNTIME_ERROR=%s\n",e.what());return 2;
    }
}
