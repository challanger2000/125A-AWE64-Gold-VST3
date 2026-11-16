# P105 — Source-derived Pentium P54C Dynarec timing, stereo allocator

**Status: partial research result; autonomous Creative Win95 register timing NOT APPROVED.** No product VST3, original EMU8000 chip, ROM or private Creative VxD is changed by P105.

## Confirmed original machine and actual CPU model

A private reinspection of the user's actual 86Box Windows-95 machine configuration (nested `VM/86box.cfg` in the owner-supplied V4 Win95 package) established:

```ini
machine = p55t2p4
cpu_family = pentium_p54c
cpu_speed = 133333333
cpu_multi = 2
cpu_use_dynarec = 1
```

According to the **pinned original 86Box AWE32 fork** [`turican0/86Box-AWE32@v6.0-AWE32-0.93`](https://github.com/turican0/86Box-AWE32/tree/v6.0-AWE32-0.93), `src/cpu/cpu.c` selects `codegen_timing_pentium` for this CPU family with Dynarec enabled. Its `src/cpu/codegen_timing_pentium.c` accounts for U/V-integer pipe pairing, register dependencies, AGI stalls and decode effects. In the non-MMX integer subset, standalone costs are REG=1, memory read=2, read-modify-write=3, and branch=2 guest cycles. **Do not infer that every original Jcc interval is exactly two cycles:** its effective contribution to a Dynarec block depends on pairing, surrounding instructions and additional CPU work.

The public original source describes U/V pairing with these costs:

| U instruction / V instruction | register | memory read | read-modify-write | branch |
|---|---:|---:|---:|---:|
| register | 1 | 2 | 3 | 2 |
| memory read | 2 | 2 | 3 | 3 |
| read-modify-write | 3 | 4 | 5 | 4 |
| branch | invalid | invalid | invalid | invalid |

These costs were brought into a **bounded, no-allocation, fail-closed C++23 research model** `src/creative_compat/P105PentiumDynarecPairing.hpp`. The model checks known register RAW hazards, U/V eligibility, and unpairability of displacement-bearing instructions. It **rejects** unverified dependency decode and AGI stalls instead of making up an x86 timing result. It does **not** model the complete 86Box 32-bit x86 Dynarec translator, chip I/O or host/chip fractional-sample-phase, so `autonomous_scheduler_ready()` is always `false`. No integration into normal VST3 audio was made.

## Authentic source-original stereo control

The owner's original physical CPU/EMU8000 V4 package **`P003-20261116T014751Z-3594ea53-ERGEBNIS.zip`**, SHA-256 `2b507624a159f4f8ed9c69e22b3a300bbbe8937aecb0d6d3b254ae641ca2f3c6`, was checked **locally**. The private archive contains the actual 86Box CPU I/O return-site / TSC / register callbacks; no part of its original binary/media/trace was committed to GitHub.

`tools/p105_original_stereo_dynarec_private.py` is a SHA-pinned, **read-only, offline** auditor. It selects precisely six original helper identities by Win95 VXD return-site and validates physical callback directions/ports, ordering and counts. Its summary result on the owner's genuine file was:

| Private original evidence | Result |
|---|---:|
| Stereo allocator executions observed | **25** |
| Repeated full scans | **9** |
| Original helper groups per full scan | **92** |
| Physical port callbacks per full scan | **245** |
| Variable TSC intervals within 91 adjacent helper transitions | **8** |
| All eight variable intervals located exactly at | `CCCA → VTFT` |
| CCCA read cursor strictly increasing in all nine passes | **8/8 variable sites** |
| Eight variable gaps *not* explainable by a single fixed end-position threshold | **8/8** |
| Actual variable guest CPU TSC gaps | **91, 93, 95 cycles** |
| CCCA internal 12-cycle callback groups | **72/72** |

For each of the eight variable positions, the **24-bit source sample cursor** strictly advances across all nine full scans, whereas the observed interval changes among 91, 93 and 95 cycles in a non-monotone order. A single fixed sample-end threshold would yield at most one transition under a monotonic cursor; **all eight original positions exceed that bound**. This is a **falsification of that single-cause hypothesis**, not proof of which exact x86 path or cache/decode/branch effects account for the residual.

The auditor's `--selftest` uses only synthetic monotonic/oscillatory controls suitable for public CI. The source-original `--original-v4-zip` run must be executed strictly privately; its output contains aggregate counts, not owner trace bytes or register words.

The latest owner-supplied research source ZIP had also independently passed **31/31 native C++23 CTests** locally before this P105 branch was created. Those tests are not original CPU time parity and are not conflated with the new tests.

## CI gates / limits

`tests/p105_pentium_dynarec_pairing_only.cpp` asserts all 12 valid 4×4 pairing costs, standalone P54C costs, U/V register hazards, branch pairing, displacement behavior, unsupported-input rejection, and 100,000 pairs without heap use. `.github/workflows/p105-pentium-dynarec-pairing.yml` builds and runs these tests on **Windows-2022 / Clang C++23** and **Ubuntu / GCC and Clang C++23**, plus safe synthetic Python negative tests.

The original 86Box CPU code is used as a documented reference, **not compiled or linked into a new CPU engine**.

**Acceptance gate remains:** NOT PROVED for independently inferred correct per-IO TSC, source Win95-vs-autonomous VST3 output register stream or exact dry PCM. The next meaningful step requires a complete decoded x86 path and reliable register/flag/memory state for the eight variable transitions, including U/V pairing and AGI, followed by genuinely new MIDI/voice original-trace holdouts. Copying measured source TSC intervals or introducing voice/preset-specific coefficients would be incorrect.

User action necessary: **none**. The P105 branch is an isolated, unmerged research proposal.
