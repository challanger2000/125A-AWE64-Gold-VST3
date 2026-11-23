# P216 — Original Win95 guest timing audit and bounded 86Box chip-frame I/O

## Scope and acceptance verdict

The genuine 1996 Creative SBAWE.VXD Windows-95/86Box CPU stream and
the unmodified 86Box EMU8000 chip register stream are available in
the owner's **private** evidence. A SHA-checked local verifier independently
compared the original CPU I/O and the original chip's register read/write
events. No owner driver binary, ROM, SBK, SF2, original WAV, raw CPU
trace or file paths to those private sources were uploaded to GitHub.

This P216 change solves an **infrastructure problem**: original EMU8000
port writes can now be deferred to their precisely requested original
chip *sample frame*, across native 64-frame processing windows and
without heap allocation in live audio. It does **not** magically
reconstruct Creative VXD x86 execution time. The regular VST3 note
scheduler still submits writes at the current frame until a source-
verified original VXD CPU path is implemented.

## Authenticated owner original Win95 CPU / register timing

Independent owner-only TSC/chip trace: **1,529,747** paired guest
CPU and EMU8000 port events, with **zero I/O port or value mismatches**
in the original CPU-to-chip pairing.

The original VM's true CPU clock was **400,000,000/3 Hz
(133⅓ MHz)**, with physical original EMU8000 at **44,100 Hz**.
Sample position derives from the rational CPU-to-chip clock, not a
constant guessed cycle increment per OUT instruction.

Paired original CPU TSC-to-chip-phase distribution, after rational
conversion and integer phase normalization:

- chip phase +1: **1,529,231** events;
- chip phase +2: **514** events;
- two outliers at phase -412, exclusively exceptional original reads,
  not evidence for rewriting nominal CPU write timing.

The **original driver has multiple distinct x86 code paths**. Measured
selector/data helper transitions include dominant per-write timings
around **62** or **105–106 CPU cycles**, among other different path
lengths. The observed chip-frame spread cannot be reproduced with
one fixed number of cycles per chip I/O or with a replay of one
preset's timestamps.

The private original P007AX bank1/program7/note60 dry SF2 test has
**304 OUT register words across eight chip frames**. The native
P215 standalone converter has **304 OUT words and 303 matching values**,
but still submits them in one frame. Original Win95 register-frame
and PCM parity therefore remain **NO-GO**.

## Original C chip wrapper changes — fail-closed timing API

The original `snd_emu8k.c` DSP is unchanged. The public
`Emu8000Box.cpp/h` wrapper gains:

- `PortWriteTimed(port, value, framesAhead, isByte)`: caller-chosen
  future chip frame with a strict four-block lookahead limit and
  finite/preallocated queue;
- only **due** operations are visible through current-frame chip reads;
- future writes survive the next 64-frame native audio block;
- a later due-now MIDI write is not blocked by an older future event;
- stable same-frame original hardware port ordering;
- no C++ `new` inside the native original chip's realtime port/render
  processing; two concurrent chip instances independently tested;
- normal legacy `PortWrite` continues to use chip-frame offset zero.

The API intentionally does **not** contain a hardcoded timestamp
sequence, preset ID, device- or user-specific reference asset,
measured-port lookup table or alternative sound synthesis DSP.

The isolated private original-ROM test and the Windows test with
a public synthetic ROM both verified 3-frame due visibility,
66-frame cross-block survival, later immediate MIDI writes,
second-chip isolation, overlong offset rejection, and no heap
allocations.

## Full exact Windows evidence

Compiled/tested source SHA: `796e6c39858e3cd26feadaf5327b63ade8db6c51`.

Windows Actions run **#45089515567**, job **#137415648612**:
**COMPLETED SUCCESS**.

- P216 original chip deferred-port/two-instance/no-heap probe: PASS;
- Steinberg Windows native VST3 validator: **47/47 PASS**;
- 125A Plugin Tester v0.2.10: **41 PASS, 0 WARN, 0 FAIL**;
- 125A Plugin Tester v0.3.3: **41 PASS, 0 WARN, 0 FAIL**;
- P213/P214/P215 original Creative SF2 generator controls and
  exhaustive 65,536-case x86 PCOD numeric test: PASS;
- P210 native original 86Box chip 31/32/33/37 SF2 physical voices,
  32-voice limit and intentionally broken alias control: PASS;
- P211 genuine Win64 VST3 37-layer direct-play vs V5
  project-restore PCM in two fresh plugin instances: PASS;
- Windows editor/GUI, MIDI state, multirate, live callback allocator,
  stereo, and failed/rolled-back installer integrity regressions: PASS.

All public Windows CI uses only synthetic ROM/SF2 assets; the owner
private original media were never included in its artifacts.

## Remaining work to establish real Creative original-chip *sound*

1. Port the actual per-branch Creative x86 CPU work paths and chip
   write deadline calculation; test against multiple independent
   original Win95 guest CPU traces, not one calibration sample.
2. Address the one remaining SF2 IP word difference without a
   one-preset correction or a global off-by-one pitch shift.
3. Compare aligned, identical MIDI note/velocity/FX and original
   Creative Win95 PCM vs the final Windows VST3. The native-original
   86Box chip alone is not an adequate proof of driver timing parity.
4. Complete real Fender Studio Pro ASIO on the owner hardware and
   independent full source audit before calling a released package
   original-accurate.

This is a **verified infrastructure improvement**, not a final
Creative-driver sound parity pass or production release.
