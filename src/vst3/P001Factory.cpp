#include "P001Minimal.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/vsttypes.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

// Distinct non-shipping product identifier. Never re-use the old 125A AWE CID.
// VST3 single-component SDK class provides both processor and controller.
BEGIN_FACTORY_DEF("125A Audio", "https://github.com/challanger2000/125A-AWE64-Gold-VST3", "")
    DEF_CLASS2(INLINE_UID(0x125A6401,0xF0014AEE,0x8000C64A,0x5F190001),
        PClassInfo::kManyInstances,kVstAudioEffectClass,
        "125A AWE64 Gold P001 (ROM Test)",0,
        PlugType::kInstrumentSynth,
        "0.1.0",kVstVersionString,
        awe64::vst3::Minimal::createInstance)
END_FACTORY
