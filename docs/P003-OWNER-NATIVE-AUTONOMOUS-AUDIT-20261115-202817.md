# P003 – Windows owner result 2026-11-15 20:28:17: autonomous timing NO-GO

**Authoritative run:** owner-uploaded
`P003-ERGEBNIS-20261115-202817-030(1).zip`
SHA256 `7eb2f1e41df2bd6b514357e83e19ef9923212ac1bb7d4eabe9637c27ad661f58`.

**Original source:** authentic real Creative Win95 86Box capture
`P003-20261115T125019Z-ca0863fd-ERGEBNIS.zip`
SHA256 `badf6e05a7d4e06909a5fde706a70f8f4cb6c346f5737176e2a130d2b794ce78`;
Original dry WAV SHA256
`35315175420592f5424bbcd9f7d0d73a151fb49989f7b8ec69aa5138c99a8e12`.

**Independent audit:** the original full WAV was opened at
source frame `1632100`, and **80,000 real stereo PCM16 frames**
were compared byte-for-byte with all three exported Windows VST3 WAVs.
In addition all **324 original C4/D4 physical note OUT writes** were
independently extracted from the original 13.9 MB EMU8000 port trace,
and compared to the three host traces, not merely to JSON status fields.
ZIP CRC and per-mode native WAV and port SHA attestations were checked.

| Mode | PCM16 channel words unequal / 160,000 | Max absolute PCM16 error | RMS error, LSB | Note OUT timing |
|---|---:|---:|---:|---|
| Original-port-clock replay | **0** | **0** | **0** | **324/324** exact |
| Independent instantaneous native | 150,585 | 8,157 | 253.852 | 30/324 exact (all ON writes same frame) |
| Autonomous 133MHz phase/cycle model | **138,091** | **2,260** | **55.397** | **245/324** exact |

The 133MHz model produces identical port+value words in identical
order **324/324**, in all 3 modes. Its **79 incorrect time positions
are ALL exactly one 44.1kHz sample EARLY**, never late:
- C4 ON: **27/152** early, 125/152 correct.
- C4 OFF: **0/8** early, 8/8 correct.
- D4 ON: **52/156** early, 104/156 correct.
- D4 OFF: **0/8** early, 8/8 correct.

First wrong autonomous native write, note OUT index **65** (zero-based):
the independently authenticated original 86Box port trace has
`native frame 140 / E22 / 0000`, but the autonomous exported
VST3 host emits at **frame 139**. The first wrong PCM16 channel word
is index **303** (relative stereo frame 151, channel 1).
The observed original WAV and native source-clocked WAV share
the same **80,000-frame PCM byte SHA256**
`485b1b215bff7811658cdd8190ce6b4d02b2606c210117449070ef7fa875c0d9`.

Observed C4 ON histogram at this original source:
`[6,6,53,87]`, autonomous current `[6,6,80,60]`.
D4 ON source `[8,8,30,93,17]`, autonomous `[8,8,65,75]`
(the current candidate incorrectly completes D4 in four rather than
five sample frames).

## What the independent CPU measurement rules out

The earlier private 86Box x86 CPU callback measurements prove that
original Creative's actual **subsample CPU dispatcher phase is NOT
specified by MIDI sampleOffset**. The currently coded autonomous
mode inserts an **unverified constant half-sample guest CPU phase**
before each native MIDI note and models its x86 instruction costs
using empirical CPU-port group estimates.

Offline phase sweep of the *currently implemented* CPU-cost estimator
(with original relative frames used ONLY as comparison target):

- This original C4 event admits all 152 frame positions exactly for
  assumed CPU phase **2233–2304 cycles** out of ~3023 cycles/frame.
- Its D4 event gets at most **152/156 positions** exactly with phase
  **2578–2600 cycles**.
- **There is no single constant phase** that can yield 324/324 exact
  port timestamps for even this single source run with current
  empirical guest-cycle costs.
- Independently captured later Win95 original CPU+EMU8000 run has
  different note phase ranges; treating the fitted phase from one
  source recording as a runtime input would be a false parity proof.

This run is a **TRUE NATIVE VST3 audio NO-GO for autonomous parity**.
It does **not** invalidate the hard bit-identical DSP benchmark when
the genuine original notes are scheduled at their original register
sample clocks. The calibrated EMU8000 core and proven source-clocked
mode must remain unchanged.

## Required next engineering work

1. Derive bulk port timing and driver work from real guest x86
   CS:EIP + TSC transaction sites, including the word-index/data
   callback distinction and CPU delay after voice selection; test
   against *unseen* original Win95 sessions and notes.
2. Resolve how the 133MHz guest CPU instruction timeline maps to the
   44.1kHz source host time and source scheduling phase. **Do not**
   insert note-specific C4/D4 timing tables or source note timestamps
   into the autonomous runtime.
3. Run exact private owner original WAV+VST3 port+PCM matching after
   each change, followed by new musical MIDI programs/presets/banks.
4. Keep the P003 release gate NO-GO until the autonomous VST3 itself
   meets the stated acceptance conditions.

No private ROM, SF2, driver, trace or audio bytes were committed in
this report. The P003 source-clock benchmark is a 160,000 sample
**partial proof**, not full-bank or arbitrary Win95 driver parity.
