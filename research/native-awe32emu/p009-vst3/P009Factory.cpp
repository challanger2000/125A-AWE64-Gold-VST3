#include "P009Native.hpp"
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/vsttypes.h"
using namespace Steinberg;
using namespace Steinberg::Vst;
BEGIN_FACTORY_DEF("125A Audio",
    "https://github.com/challanger2000/125A-AWE64-Gold-VST3",
    "P009 NATIVE RESEARCH ONLY")
    DEF_CLASS2(INLINE_UID(0x125A8600,0xF0094AEE,0xB64A0000,0x00000901),
        PClassInfo::kManyInstances,kVstAudioEffectClass,
        "125A AWE64 Gold Native P009 TEST",0,
        PlugType::kInstrumentSynth,"0.0.1",kVstVersionString,
        p009::NativeVst3::createInstance)
END_FACTORY
