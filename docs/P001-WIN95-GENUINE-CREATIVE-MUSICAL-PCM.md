# P001 – independent genuine Creative Win95 MIDI-to-EMU8000 PCM evidence

**2026-11-11: Independent *musical* driver/VM chip-output parity PASS.**

The initial 86Box Win95 boot recording, with 92,887 port events and
6,507,200 equal PCM16 samples, was almost silent (only 15 nonzero samples).
It was **not** accepted as proof of driver note playback.

The private 512-MiB Windows 95 + Creative AWE64 Gold VM disk was cloned;
the clone alone was modified by inserting our small CRT-free
`P001MID.EXE` at the FAT16 root. Win95 then booted inside the real, pinned
`turican0/86Box-AWE32` emulator, running original Creative driver
`SB AWE32 MIDI Synth [620]`. Guest driver code executed unmodified.
The helper called `midiOutOpen`, original WinMM program 0, then
NoteOn/NoteOff for MIDI keys **48, 60, 64, 67, 72**, with velocities
**100, 96, 96, 96, 108**. The *guest's own Win95 disk logfile*
`C:\\P001MID.LOG` confirms all 5 NoteOn and 5 NoteOff WinMM calls
returned `0x00000000`. It did not persist a final normal-exit
marker; **normal program termination is therefore not claimed**.

Crucially, the *running fork* independently captured actual
`EMU8K_TRACE` and chip-tap `AWE32_WAV` over a **140-second private
VM experiment**. The capture contains musical PCM (not just a boot pop)
in 44.1-kHz sample-clock seconds 66–70 and real port writes in that span.

## Actual numeric evidence from real private experimental run

| Property | Measurement |
| --- | --- |
| Fork C source commit | `fee1e81977824fd2d5eed4d239589dc30d251dd7` |
| Unmodified DSP Git blob | `e60eb10f00a8e1187380faa25989f6ba0433d99a` |
| ROM SHA-256 | `4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e` |
| EMU8000 sample RAM | 28,672 KiB (28 MiB) |
| Genuine original WinMM NoteOn/Off | 5 + 5, zero WinMM errors |
| Guest trace writes / reads | **49,282 / 44,163** |
| Total guest register events | **93,445** |
| Original fork WAV frame count | **6,430,760 stereo frames** |
| Interleaved stereo PCM16 samples | **12,861,520** |
| Original fork nonzero samples | **398,777** |
| Peak original PCM16 | **6,893** |
| Original fork WAV SHA-256 | `d976f51d30759f2feffab3ca97025d33c57e8ab5e210e6f0f9e32edfb10237f3` |
| New native replay WAV SHA-256 | **exactly same hash** |
| Guest register trace SHA-256 | `9b6442f5ba348eca9f698b1d8ec8736a43c052258229d9b5dd95ef0699bc8e49` |
| First PCM mismatch | **none, across all 12,861,520 samples** |
| Destructive PCM mutation | A single sample changed → **fail** at frame 2,914,796, left channel (re-run may select the same first audible sample) |

Reproduced privately with:

```sh
python tools/p001_guest_capture_gate.py \
  --rom PRIVATE/awe32.raw \
  --trace PRIVATE/fork.trace \
  --fork-wav PRIVATE/fork.wav \
  --guest-log PRIVATE/P001MID.LOG \
  --replay PRIVATE/awe64_replay \
  --private-out PRIVATE/new-unique-check-dir \
  --negative
```

The test is **fail closed**: checks ROM SHA, exact guest driver endpoint
and delivered NoteOn/Off events, significant contiguous nonzero musical
audio, the presence of guest EMU8000 writes during audible output, trace
ordering, every PCM16 sample via the unchanged original C engine, identical
WAV digest, and a one-bit mutated actual note that must be rejected at the
first differing sample. It writes only to a new, private output directory.
The existing private driver/ROM/Win95 image/trace/WAV bytes are never
committed or uploaded.

## Scope and explicit non-claims

This establishes **the original creative Win95 driver's concrete five-note
program-0 register sequence reproduces bit-exact DIGITAL chip audio in
125A's original-core adapter**. This is not a proof that a different
program, polyphonic voice allocation, controller, SF2/SBK upload or every
Creative SoundFont sounds right. Nor is it physical AWE64 Gold LINE OUT,
analog post-mixer, Studio One, ASIO, glitch-free realtime, or released
commercial product fidelity. The corresponding P002/003 and P004 acceptance
gates remain independent.

P001's independent *musical* ROM/driver chip-output gate is **now PASS**,
rather than the previous NOT RUN. Overall issue stays OPEN/Draft PR until
remaining P001 realtime safety and the complete smoke-acceptance evidence
have been assessed.
