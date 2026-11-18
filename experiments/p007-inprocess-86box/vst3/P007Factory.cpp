#include "P007Native.hpp"
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/vsttypes.h"
using namespace Steinberg;
using namespace Steinberg::Vst;
BEGIN_FACTORY_DEF("125A Audio",
    "https://github.com/challanger2000/125A-AWE64-Gold-VST3", "")
    DEF_CLASS2(INLINE_UID(0x125A8600,0xF0074AEE,0xB64A0000,0x00000701),
        PClassInfo::kManyInstances,kVstAudioEffectClass,
        "125A AWE64 Gold",0,
        PlugType::kInstrumentSynth,"0.0.2",kVstVersionString,
        p007::Plugin::createInstance)
END_FACTORY
