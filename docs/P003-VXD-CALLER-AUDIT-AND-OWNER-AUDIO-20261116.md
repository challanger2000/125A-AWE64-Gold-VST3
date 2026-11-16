# P003 – Original Creative SBAWE.VXD opcode and static caller audit, 2026-11-16

**Source:** the independent PR #4 forensic finding
[issuecomment-6900406458](https://github.com/challanger2000/125A-AWE64-Gold-VST3/pull/4#issuecomment-6900406458)
and the owner-private analysis package. The actual original `SBAWE.VXD`
(SHA-256 `d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`)
was used locally. **No original binary, ROM, bank, or audio contents appear here.**

## 12 opcode anchors are authentic; they are helper addresses, not the MIDI callers

The verified LE/LCOD source object contains six x86 helpers for byte,
word and double-word IN/OUT; the twelve identified opcode sites correspond
to genuine `OUT DX,AL`, `OUT DX,AX`, `OUT DX,EAX`, and matching input
instructions. A guest 32-bit OUT is two EMU8000 physical 16-bit callbacks.
816,583 of 816,594 original CPU/EMU8000 physical events were associated
with these helper instruction locations; eleven initial/other events were
not attributed. Those events do not establish *which code called the helpers*.

A new source-only private auditor
[`tools/p003_private_vxd_static_callers.py`](https://github.com/challanger2000/125A-AWE64-Gold-VST3/blob/develop/p002-creative-register-oracle/tools/p003_private_vxd_static_callers.py)
verifies the pinned VXD SHA, reconstructs its LE LCOD and checks opcode
anchors. A raw scan for x86 direct `E8 rel32` CALL instructions targeting
the six I/O helper function *entries* yields the following **candidate**
addresses, without any original source bytes committed:

| Original I/O helper class | Possible direct CALL sites |
|---|---:|
| out byte | 5 |
| in byte | 0 |
| out word | 114 |
| in word | 8 |
| out double-word | 107 |
| in double-word | 25 |
| **Total possible direct E8 helper callers** | **259** |

**Critical limitation:** a raw `E8` match is not sufficient to prove
an instruction boundary or that the instruction executed in the original
Win95 guest. Adjacent `PUSH EBP; MOV EBP,ESP` signatures provide hints,
not proven function boundaries. Dynamically attributable call-stack
return addresses or genuine CFG recovery are needed before the caller
counts can be used to reconstruct the creative MIDI voice-selection
algorithm. No claim that all 259 candidate addresses were executed.

The public synthetic regression
[`test_p003_private_vxd_static_callers.py`](https://github.com/challanger2000/125A-AWE64-Gold-VST3/blob/develop/p002-creative-register-oracle/tests/test_p003_private_vxd_static_callers.py)
covers relative CALL decode, wrong-private-image rejection, duplicate
targets, suspicious embedded-byte false positives and prologue caveats;
private VXD is **not** an input to GitHub CI.

## Owner Windows VST3 regression submitted 2026-11-16 01:00 CET

Input: `P003-ERGEBNIS-20261116-010037-143.zip`.
Three actual native VST3 host runs and their WAV/OUT files were examined,
including a byte-for-byte comparison of the newly supplied test's WAVs
against the immediately preceding owner test.

| Experiment | Before | New |
|---|---:|---:|
| Original-clocked reference, PCM16 words wrong | 0/160000 | 0/160000 |
| Autonomous PCM16 words wrong | 138091/160000 | **138091/160000** |
| Autonomous native OUT times wrong | 79/324 | **77/324** |
| Real exported VST3 binary SHA-256 | — | `09e627d576d8bf72780f247a32c82bfa1eedefbc4e247f037d7dcb7fd21c4d12` |

The autonomous WAV is **byte-identical** with the previous owner run:
the new timing improvements affected two OUT locations (indices 90, 91),
but did not improve the PCM bytes. The actual VST3 autonomous mode remains
**NO-GO** and should not be called bit-identical or product-ready.

## Required further work

1. Recover *executed* original x86 caller return addresses or reliably
   reconstruct helper-call CFG edges and voice scheduler state transitions.
2. Separate variable original Win95 guest sub-sample start phase from
   reproducible helper work; do not fit one C4/D4 sound/preset table.
3. Update original-compatible native state handling and generic CPU work
   independently; do not touch the pinned original EMU8000 core.
4. Re-run owner real Win95/VST3 WAV and port differential on independent
   MIDI pitches, voices, velocities, controllers, programs and SF2 banks.
   Preserve the proven 160,000/160,000 source-clocked dry PCM case.

**Release gate: NO-GO for autonomous Win95-driver timing and PCM parity.**
