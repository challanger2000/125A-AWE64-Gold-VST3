# P009 — genuine native Windows VST3 research instrument

The plugin integrates reconstructed Creative Win95 driver logic and
86Box-derived EMU8000 audio core directly as a Windows x64 VST3.
There is NO 86Box VM, emulated PC, guest Windows95, virtual disk
or original VxD process. Sources pinned to original upstream commit
17b0578fe59909dfaa784b5531d6d1a3a80729ce.

This is in isolated P008 research branch, not the working P007.
Plugin ID differs from current AWE64 Gold. Interface supports 16
MIDI channels, automatic CH10 GM percussion, CH1 audition via browser
drum mode, program/bank automation, stereo output and project state.
Native 980-frame latency remains and is reported.

For PRIVATE testing place actual user files in this bundle subfolder:

    AWE64_Gold_Native_P009.vst3/Contents/Resources/P009-Private/
        awe32.raw
        SYNTHGM.SBK
        user.sf2          (optional owner SF2)

Never upload private ROM/SBK/SF2 to public GitHub. Without authentic ROM
and bank, the VST3 is intentionally silent (not a false audio PASS).

**PROTOTYPE / RELEASE NO-GO**: Upstream bank-zone selection and queued
EMU8000 port writes allocate memory on the audio callback. The
86Box-derived chip has process-global timing state, and true
multi-instance independence under DAW thread migration is NOT proved.
Host MIDI CC/RPN/NRPN, SF2 hot load GUI, and the finished P165 frontplate
are not implemented. Studio Pro audition is deferred until full QA.
Public CI confirms compilation and SDK validator only.

License: EMU8000 C core is GPL-2.0-or-later; remaining AWE32Emu source
does not yet declare a redistribution licence. A public binary release
needs author permission and GPL corresponding-source compliance.
