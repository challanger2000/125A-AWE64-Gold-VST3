# P007 guest component: not a second host program
This Windows 95 PE32 helper runs **inside the private emulated Win95 disk**, not as an external Windows 11 EXE or installer.

The helper selects the original Creative AWE/EMU8000 WinMM midiOut device. It refuses Microsoft synth/MPU-401 fallback. Once COM2 is open, it emits the exact READY token over original emulated UART transmit; the in-process VST3 core gates MIDI on reception. On WinMM send error it emits STOP.

The executable must be installed in the PRIVATE guest image's Win95 Startup directory in advance; the public repository intentionally contains neither licensed Win95 nor the original Creative binary nor guest disk. A verified automatic prepackaged private boot image is still required for a finished one-folder VST3.

Not yet proved: real guest boot / Creative MIDI / original EMU8000 PCM / ASIO. No claim that the emitted READY token was received from a real guest in CI.
