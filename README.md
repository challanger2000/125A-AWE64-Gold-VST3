# 125A AWE64 Gold VST3

Hardware-faithful Windows-x64 VST3 synthesizer targeting the **Creative Sound Blaster AWE64 Gold**, built around the **unaltered, hardware-calibrated EMU8000 implementation** from [turican0/86Box-AWE32](https://github.com/turican0/86Box-AWE32).

> **Status: development bootstrap. No working VST3 or sound-fidelity claim yet.**

## Start here

**[START-HERE.md](START-HERE.md)** is the binding technical development brief. It specifies the pinned upstream source, architecture, original-Creative-driver comparison strategy, acceptance gates, and licensing checks.

Upstream release: `v6.0-AWE32-0.93` · commit `fee1e81977824fd2d5eed4d239589dc30d251dd7` · EMU8000 source blob `e60eb10f00a8e1187380faa25989f6ba0433d99a`.

## Design rules

- Preserve the original fork DSP implementation without custom filter, envelope, interpolation, gain, or effects patches.
- Reconstruct original Creative MIDI/SF2/ROM/register behavior using actual Creative driver traces from the existing Windows 95 + 86Box-AWE32 setup.
- Compare digital PCM under equal chip conditions before claiming fidelity. Check physical AWE32/AWE64 Gold recordings separately.
- Reuse the established 125A GUI only **after** chip/driver/audio acceptance.
- Never bundle Creative ROMs, proprietary drivers, or user SoundFonts.

## Licensing

Upstream 86Box-AWE32 identifies itself as GPL-2.0. Source redistribution and VST3 dependency compatibility must be reviewed before integrating and distributing binaries. Selling GPL-based software is possible when its obligations are met.

The previous [125A-AWE-Synthesizer](https://github.com/challanger2000/125A-AWE-Synthesizer) remains available as historical reference, not the new audio-engine base.
