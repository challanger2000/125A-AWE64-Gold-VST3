# 125A AWE64 Gold native P165 – adversarial source/host audit

**Date:** 20 November 2026  
**Audit verdict:** **PARTIAL / NOT VERIFIED** against `125A-Engineering/QA/AUDIT-COMPLETENESS-CONTRACT.md`; **release recommendation: NO-GO** until findings corrected and remaining gates executed.  
**Product under audit:** `challanger2000/125A-AWE64-Gold-VST3`  
**Read-only production baseline:** `feature/native-p165-complete-integration-20261120@8a75964edf4e273b9b8730ef8b47f9788a38c501` (the user's P165 final private-bundle source, unchanged by the audit)  
**Audit-only test branch:** `audit/p165-adversarial-readonly-20261120@670626ebddf6fd59419302a7f3cdaa7d998ef57a`; this branch changes **only** one host test and its CI branch trigger.  
**Accepted older native baseline:** `research/awe32emu-native-isolated-20261120@280dbdd4e60d88aed83c5df6b538da18e0cff957`  
**Last Win95/P007 approved GUI source:** `feature/p007au-original-creative-sf2-guest-loading-20261119@a74998b735632fb9ee8827e39d8320509b50a692`  
**Upstream engine:** `turican0/AWE32Emu@17b0578fe59909dfaa784b5531d6d1a3a80729ce`, pinned original 86Box-based chip source, modified at build by the listed RT/resampler/GUI overlay scripts.  
**Scope:** ONE Windows x64 native VST3 and P165 editor; no guest Win95/86Box executable. The older P007 remains a reference and was not modified. No physical AWE64 card is available and hardware playback is **not a required gate**.

## 1. Applicable instructions and evidence

The following instructions were obtained in full from the engineering repository and read: `START-HERE.md`, `README.md`, all **2,922 lines** of `ENGINEERING-GUARDRAILS.md`, `QA/AUDIT-COMPLETENESS-CONTRACT.md`, `QA/RELEASE-QA.md`, `QA/REALTIME-QA.md`, `QA/AUDIO-FIXTURES.md`, `STANDARDS/PROFESSIONAL-PLUGIN-BEHAVIOR.md`, `STANDARDS/GUI-GUIDELINES.md`, `STANDARDS/SOURCE-BASELINE-GATE.md`, `STANDARDS/DSP-GUIDELINES.md`, `STANDARDS/REPOSITORY-WORKFLOW.md`, `STANDARDS/AI-ASSISTED-ENGINEERING.md`, `STANDARDS/BRANDING.md`, `STANDARDS/DEMO-LICENSING.md` and `RESEARCH/SOURCES.md`. The product's `START-HERE.md` and relevant P009/P015/P022 notes were checked. Historical and generated sources can contain stale status statements, which are not interpreted as current test results.

Actual artifacts checked:
- The user's latest **private** `AWE64-GOLD-ABSCHLUSS.txt` ends with `AWE64_GOLD_FINAL_TEST_EXITCODE=0`; real private Windows original-ROM/SBK/77 SF2 host PCM, 8/8 750ms original-chip-rate comparisons and existing V1–V5 state *subsets* passed. This is important **positive evidence**, but not an exhaustive state or DAW gate.
- [Original production-branch Windows CI #44809518766](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44809518766): **PASS**, 47/47 Steinberg validator, specific no-heap test, multirate native chip comparison, P165 EditorHost pixel/gesture test, and the then-existing V5 host state checks.
- **New independent Windows counterprobe** [audit-only CI #44815816560](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44815816560): **FAIL for the intended regression** (see F-01). Prior positive controls/validator ran and passed. No production source was altered.
- Actual owner private distribution ZIP `125A-AWE64-Gold-P165-NATIVE-FINALES-GESAMTPAKET-PRIVAT.zip`: archive CRC **PASS**; embedded Win64 VST3 binary SHA256 matches its provenance; XML resources/faceplate/ROM/SBK/SF2 are present **inside the private file only**; the bundle and metadata were checked without publishing protected bytes. The original P165 GUI screenshot was inspected visually.

**Evidence categories:** source findings below distinguish directly proven static faults, executed Windows results, and unexecuted hypotheses. This is **one reviewer's self-challenge**, not an independently commissioned second reviewer.

## 2. Source and interaction coverage inventory

| Surface / files | Input -> effect -> state | Review / executed scope | Verdict |
|---|---|---|---|
| VST3 factory `P009Factory.cpp`, CMake, moduleinfo | FUID, name, instantiation, stereo bus, resources | Complete key source and real ZIP metadata; real validator PASS | **PASS** basic instantiation; **FAIL** release identity consistency |
| Processor/host `P009Native.cpp/.hpp` | MIDI, program, bank, params, state, lifecycle, audio buffers | Full native file inspected, explicit code-path traces; user positive PCM, audit counterprobe | **FAIL** V5 advanced recall and unsynchronized state |
| GUI bridge `P009NativeGui.cpp` | custom VSTGUI, SF2 selection, parameter mirrors | Complete file inspected; EditorHost PASS, no final DAW | **INCONCLUSIVE** GUI-to-audio chain |
| Hardware GUI `p165/PresetBrowserView.cpp/.h` | 12 knobs, two effect type menus, browser, ROM/SF2, reset, master | Full view source inspected and real EditorHost screenshot checked | **FAIL** visible FX menus disabled |
| Browser `PresetBrowserModel.h`, `PresetCatalog.h`, `IbkBank.h` | genuine sparse SF2 phdr, ROM, bank/program | Full header implementations inspected; private 77-entry positive PCM | **PASS** tested catalog subset; other malformed-bank/failure cases **NOT RUN** |
| Original FX `P009CreativeFx.hpp`, P175 overlay script | chip INIT/HWCF, amp/filter/sends | Full integration glue inspected; state-only FX types PASS | **NOT RUN** independent changed-type PCM, zero-send audibility |
| External assets `P009AssetFingerprint.hpp` | SHA256, path validation, bank restore | Full source inspected; malformed-path negative in host | **NOT RUN** full positive V5 external-SF2 DAW save/reopen PCM |
| Original 86Box chip and driver `_p009-upstream/AWE32Emu/src/{Synth,SoundFont,Emu8000,Emu8000Box,86box/snd_emu8k.c}` | driver/voice/register/PCM | Pin & source patches inspected; only relevant upstream sections; **not all transitive upstream code read line-by-line** | **NOT VERIFIED** full DSP/source-review coverage |
| Build/source transforms `apply_per_instance.py`, `rt_safe.patch`, `p019_latency_window_probe.py`, `p021_box86_native_resampling.py`, `apply_p165_original_synth_overlays.py` | per-instance timing, 64-frame window, RT bound, all-rate PCM | Scripts/diff inspected; original CI passes | **PASS** build and defined tests, **NOT VERIFIED** stress/deadline coverage |
| Host `tests/P010NativeHostQA.cpp` | synthetic and private ROM/SF2, V1–V5, MIDI, PCM | Key assertions and all private main branches inspected; new counterprobe inserted | **FAIL** test completeness for V5+RPN and external path |
| GUI `tests/P165NativeEditorHostQA.py` | actual EditorHost, window screenshots, clicks | Complete test inspected and evidence read | **PASS** visible controls and tested actions; **FAIL** FX menu input coverage |
| Other source tests `P009NativeInstanceProbe`, `P009RealtimeAllocationProbe`, `P016PublicRecallPCM`, `P019ChipLatencyProbe`, `P019ComparePCM`, `P021OriginalChipResamplingProbe`; related SF2 fixture scripts | native multi-instance, heap, latency, PCM | Identified from CMake and scripts; CI outcomes checked; **not every implementation line and mutant inspected** | **NOT VERIFIED** complete source/test review |
| End-user package and Windows-only helpers | one VST3, private assets, BAT, final evidence | Actual private ZIP structure/BAT, moduleinfo/provenance/CRC checked | **FAIL** public release metadata/legal gate; private battery of tests PASS |
| 125A Plugin Tester + final Fender Studio Pro 8 | actual install/reopen, automation, stress, GUI, musical sound | Neither tool executed in this audit | **NOT RUN** |

**Inventory declaration:** 14 grouped production/test surfaces above, with 12 physical P165 mod controls, 2 FX type selectors, master, bank/program menus, previous/next, SF2/ROM buttons, reset, browser search/filter/selection and two pages traced to their code. The *full* transitive external chip source, all auxiliary test implementations and release DAW are uninspected/unexecuted. Therefore the mandatory **SOURCE REVIEW COMPLETE / FUNCTIONAL AUDIT COMPLETE / FULL AUDIT** gates are not satisfied.

## 3. Adversarial findings

### F-01 — HIGH / EXECUTED FAIL: V5 silently destroys MIDI advanced-controller and RPN state

**Production:** `research/native-awe32emu/p009-vst3/P009Native.cpp:907–914`, particularly:
```cpp
if(s.version==kStateV4) {
    advancedMidiState_=s.advancedStates;
    selectedParameterMode_=s.parameterModes;
}
```
V5 stores `s.advancedStates` and `s.parameterModes` in the same backward-compatible 580-byte prefix, but the restore branch handles **only V4**, not V5. Any P165 GUI edit or selected external SF2 forces V5 and wipes saved CC0/RPN/NRPN state to 0xFFFF/zero on reload.

**Executed safe negative, actual Win64 Steinberg VST3 host:** [CI #44815816560](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44815816560). The **positive control** loads and roundtrips original V4 with CH1 RPN 0/0 bend range 12. Then one legitimate AMP Attack change forces V5; valid V5 is restored to a fresh instance. The test reports exactly:
```
AUDIT_NEGATIVE_V5_RPN_RECALL_LOSS first_differing_byte=312 input=0 restored=255
P010_FAIL AUDIT_FAIL P165 V5 LOST ACTUAL 16-CHANNEL RPN/NRPN MIDI STATE
```
This is a genuine **detected regression**; the failing audit CI is expected and must not be disguised as a plugin PASS. Existing green V4 tests and V5 GUI-only tests never combine these two dimensions.

**Fix required:** restore advanced state for V4 **and V5**, and test V5 with RPN/bend, banks, 16 channels and private PCM post-reopen. No production code was changed.

### F-02 — HIGH / SOURCE-PROVEN GUI FAIL: reverb and chorus type menus are visually present but mouse-disabled

**Production:** `p165/PresetBrowserView.cpp:541–559, 584–589`. The constructor explicitly executes
```cpp
reverbMenu_->setMouseEnabled(false);
chorusMenu_->setMouseEnabled(false);
```
**twice**, despite later `valueChanged()` handlers for IDs 1400/1401 and actual chip FX register code. The user **cannot select the advertised eight reverb/chorus types through these menus**. `P010NativeHostQA` calls `setParamNormalized(1400/1401)` programmatically and verifies V5 state; that does not exercise the disabled GUI. `P165NativeEditorHostQA.py:381–390` counts colored pixels for both menus but **never clicks them**.

**Counterchallenge:** the actual menu remains visible in the screenshot when mouse input is disabled. Therefore the existing screenshot PASS and the parameter-state PASS are compatible with this real UI defect. A real GUI pointer/keyboard selection followed by changed original-chip PCM, state recall and reopen is **NOT RUN**. Do not count FX type mouse handling or audible FX change as PASS.

### F-03 — HIGH / SOURCE-PROVEN CONCURRENCY HAZARD: state save/restore and render share mutable non-atomic state

**Production:** `P009Native.cpp:424–455, 707–739, 749–759, 899–969`; `P009Native.hpp` corresponding members. The audio callback writes `program_`, `bank_`, arrays `midiState_`, `advancedMidiState_`, channel program/bank, master and `guiControls_`. `getState()` reads these without an atomic snapshot, lock or mailbox. A DAW can save while processing. `setState()` additionally calls `synth_->SetGuiControl`, controller resets and bank replay on its caller's thread after changing the same fields.

These source paths are **not race-safe by construction** if host state calls overlap the audio callback. The currently green multi-instance test does not establish same-instance concurrent getState/setState and process under TSAN/ASan or a realtime host scheduler. **Crashes or actual mismatched samples have NOT been reproduced**; dynamic race/lifecycle challenge remains **NOT RUN**. Required: establish a single writer, serialized host lifecycle, and a coherent atomically published snapshot/command-transfer strategy; exercise concurrent save/reopen in the actual host.

### F-04 — HIGH / TEST-ORACLE FALSE POSITIVE: `AWE64_GOLD_USER_SF2_RESTORED` is NOT a V5 external-file DAW recall test

**Actual delivered `START-AWE64-GOLD-ABSCHLUSSTEST.bat`:** After private host tests it simply copies `77-presets.sf2` back to bundle-local `user.sf2`, and emits `AWE64_GOLD_USER_SF2_RESTORED`. No V5 GUI file-picker path, SHA-positive DAW project reopen, or equality of audio after project reopening is executed by that marker.

`P010NativeHostQA.cpp:108–213` tests 12 GUI-value V5 roundtrip with **empty user path** and rejects a fabricated malformed external path. Its private ROM/SF2 A/B/A tests in `main:1082–1151` instead replace the **fixed bundle-local `user.sf2` on disk and create new plugin instances**, which cannot validate a real external SF2 path selected in the GUI. Thus real-asset **positive** V5 file-identity/recall (same path, different bytes, missing file, restored PCM, GUI labels) remains **NOT RUN**, notwithstanding previous overall PASS messages.

### F-05 — MEDIUM/HIGH / SOURCE-PROVEN CONTROL LOGIC RISK: unchanged-bank reselection callback never reasserts the processor bank

**Production:** `p165/PresetBrowserView.cpp:735–762` states that selecting an unchanged VST3 bank after MIDI CC0 must send an explicit message to the processor or the host may coalesce the unchanged parameter. However `P009NativeGui.cpp::createCustomView` supplies only
```cpp
[](std::uint16_t bank){return bank<=2u||bank==128u;}
```
This returns `true` without **any** MIDI/processor command. `P009Native.cpp::applyAdvancedController` can change the chip-side bank via MIDI CC0 while the P165 UI still reads `controller_->getParamNormalized(kBankParam)`. The browser therefore has a known **UI/audio desynchronization path**, and a repeated same-bank click may be lost. No direct UI+CC0+real-SF2 PCM end-to-end negative test exists. Treat the exact host-dependent mismatch reproduction as **NOT RUN**, but the missing claimed reassertion as **confirmed by source**.

### F-06 — HIGH / CONTRACT MISMATCH: 0% hardware reverb/chorus SEND is neutral, not guaranteed zero

**Production:** `apply_p165_original_synth_overlays.py:138–152` modifies voice `reverbSend` and `CSL` only if `m_guiControls[10/11] > 0.f`. With the visible SEND knob at 0%, the sound retains the original SF2/Creative preset send levels, which **may be nonzero**. The 125A user contract for send controls is **0% send = 0**, while the original tone-modification contract says 0% = unmodified original preset. These distinct semantics were conflated. The code does **not enforce zero send** at 0%, even though preserving the original PCM at neutral 0 is intentional. A per-voice chip-send 0% negative PCM test is **NOT RUN**. Resolve the product semantics without silently normalizing/changing original ROM PCM.

### F-07 — MEDIUM / SOURCE-PROVEN LIFECYCLE OWNERSHIP RISK: queued Synth pointers not cleaned by terminate()

`P009Native.cpp:255–258` only does `synth_.reset(); active_=false;`. The raw staged/retired synth pointers and pending catalog are cleaned by `cleanStagedAndRetiredNonRealtime()` in `setActive()`, **not in terminate or a declared destructor**. If a user requests a SoundFont while the instance is inactive and then closes the plugin without a matching active/deactivation cycle, `requestUserSf2NonRealtime` can leave a fully built 28-MiB `stagedSynth_` owned by a raw atomic pointer. The C++ atomic destructor does not delete it. Conditional leak is supported by the code; memory-leak measurement in a real host is **NOT RUN**. Add final idle/inactive/teardown ownership test.

### F-08 — HIGH RELEASE BLOCKER: incomplete license provenance and private-package release identity

Actual source dependency `turican0/AWE32Emu` README identifies `snd_emu8k.c` as **GPL-2.0-or-later** and explicitly states **the rest of the project has not had its license decided**. The native VST3 links that external `Synth.cpp`, `SoundFont.cpp` and derivative GPL core. Public distribution therefore needs a resolved permission/license analysis, GPL-compliant corresponding-source/notice packaging and Steinberg-SDK compatibility assessment. The user's **private test ZIP is not itself evidence of distributable license compliance**. Do not assert distribution is permitted.

The actual user private ZIP is named `AWE64_Gold_Native_P009.vst3`. Its generated `moduleinfo.json` has `Name: AWE64_Gold_Native_P009` and `Version: 0.0.1`, while class metadata says `125A AWE64 Gold` version `0.9.0`. A strict JSON decoder rejects its trailing commas (Python JSON parser: `Illegal trailing comma before end of object`); the Steinberg validator did pass, and actual host compatibility impact is **NOT VERIFIED**. The ZIP contains no GPL LICENSE/COPYING/source offer (reasonable for a strictly private test, not a public distributable release). The VST3 identity/version mismatch must be resolved before a labeled final release.

### F-09 — MEDIUM / SOURCE-LOGIC RISK: 512-event hard failure may discard NoteOff without recovery

`P009Native.cpp:662–686` rejects a block with more than 512 input events by zeroing current output and returning `kResultFalse`. It does not issue AllNotesOff, drain pending NoteOff, or preserve a future recovery queue. Existing active EMU8000 voices may continue when later processing resumes, potentially producing stuck notes after an overflow. The synthetic/owner tests establish **no dynamic overload/NoteOff recovery negative**; outcome is **NOT RUN**. Validate error handling for the host's worst supported event bursts.

## 4. Bidirectional requirement / evidence matrix

| Req | Externally observable contract | Implemented path | Positive evidence | Adversarial / negative / boundary | Result |
|---|---|---|---|---|---|
| R01 | Actual ROM/SBK playback | Load ROM/SBK -> 28MiB sample RAM -> EMU8000 -> VST3 | Owner private host PCM 44.1k audible 16 channels | ROM missing public silence PASS; full RAM upper/lower boundary NOT RUN | **PASS tested sample subset** |
| R02 | 77 true SF2 programs, hole63 not artificial | SF2 phdr & loaded native driver | Owner 77/77 real sound PCM, missing63 ROM fallback | malformed/duplicate phdr catalog guards reviewed; invalid bank matrices not executed | **PASS tested set** |
| R03 | Correct original-chip sample-rate conversion | 44.1k chip/linear resampling | P022 8/8 equal 750ms dry/FX, public and private | Original 44.1k reference uses **same** 86Box chip; external Win95 PCM/reg parity for varied programs not performed | **PASS interpolation equivalence only** |
| R04 | VST3 sample-offset MIDI and channels | `process` queues, channel map and voice events | MIDI 16ch/CC/pitchbend/RPN and timed 44.1k patch tests | Non-44.1k 16th-rate MIDI phase and overflow NoteOff recovery NOT RUN | **INCONCLUSIVE** |
| R05 | P165 approved LCD/faceplate/dark islands | Original images, LCD x370, alpha186, real EditorHost | Actual Windows 1000x708 screenshot and interactive knobs PASS | Packaging color fallback test and full accessibility/focus NOT RUN | **PASS visually tested** |
| R06 | Twelve physical knobs change chip audio and recall | P165 `1100..1111` -> process -> P175 generator overlay | Windows host state values PASS; EditorHost AMP Attack sprite drag PASS | Every knob's positive/zero/mid/high real PCM change and V5 reopened audio NOT RUN | **INCONCLUSIVE** |
| R07 | 8× reverb/chorus TYPE menus user-operable and audibly change original chip | GUI tags 1400/1401 -> original INIT/HWCF | Programmatic parameter values/reopen PASS | **menus explicitly disabled**; no UI clicks, actual changed-type PCM NOT RUN | **FAIL** |
| R08 | 0% Reverb/Chorus SEND is absolute zero | Original chip voice register | No validated zero-send register proof | Overlay 0 leaves original nonzero source send untouched | **FAIL contractual code path** |
| R09 | ROM/SF2, bank/program, prev/next, GUI and MIDI agree | P165 browser->param 1000/1002 and CC0 | EditorHost visual prev/next/browser, private bank0/1 PCM | CC0 then same-bank GUI reselect lacks dispatch; unrun real end-to-end | **INCONCLUSIVE / defect identified** |
| R10 | SF2 GUI external file loading and accurate metadata | Win file dialog->SHA->candidate Synth->staged swap->catalog | Source implements path, malformed-path rejection exists | Real external file picker **positive**, save/reopen and actual PCM not executed | **NOT RUN** |
| R11 | Full MIDI/RPN state preserved with any GUI edit | V5 state -> next instance -> original driver | V4 positive, V5 GUI-only positive | **New independent actual Win64 V4+V5 RPN negative failed at byte 312** | **FAIL** |
| R12 | Save while playing / simultaneous load/recall no race | GUI / host / render shared fields | Separate-instance RT/alloc stress subset PASS | Same-instance concurrent getState, setState, process NOT RUN; unsynchronized fields | **NOT VERIFIED** |
| R13 | Correct 64-frame VST3 latency at 44.1k | `Emu8000Box::kLatencyFrames` -> host | Actual public host 12/64/70/140/279 sample values | Rapid sample-rate switching while active, deadline misses NOT RUN | **PASS declared values; performance NOT VERIFIED** |
| R14 | Actual portable full VST3 bundle and version | CMake/Factory/Resources/moduleinfo | Bundle integrity and DLL SHA PASS | Product naming/version inconsistency, malformed strict JSON, license notices | **FAIL release gate** |
| R15 | Independent Test/DAW gate | 125A Plugin Tester and Fender Studio Pro 8 | None in this audit | Neither executed | **NOT RUN** |
| R16 | No private Creative material in public GitHub | Owner ROM/SBK/SF2 only private ZIP | Production tree reviewed; CI public fixture has no ROM | Independent Git-history/remote audit not exhaustive | **PASS current tracked source scope; history NOT VERIFIED** |

## 5. Why earlier ALL PASS did not find the failures

1. **State partitioning:** V4 advanced MIDI and V5 GUI were tested **separately**; none tested the crossing of a saved RPN 12 semitones with a P165 knob or external bank (F-01).
2. **Visual test oracle:** The EditorHost script counts gold pixels in disabled FX menus; this proves appearance, **not input hit-testing, MIDI/DSP effect or state** (F-02).
3. **Parameter-state equals no audible proof:** Reverb/Chorus menus and 12 knobs were mainly checked by direct `setParamNormalized` and V5 serialized bytes, not by actual original-chip PCM change and post-reopen equality.
4. **Misleading summary marker:** `AWE64_GOLD_USER_SF2_RESTORED` proves restoration of the bundle-local *test-file copy*, not external-SF2 DAW project recall (F-04).
5. **Circular reference scope:** P022 is an excellent comparator for 44.1k-derived **resampling consistency**, not an independent physical-card/Creative original-driver chip/patch/audio reference.

New executed adversarial test `auditV5DoesNotEraseAdvancedMidiControllers()` contains its own **positive V4 control** and **negative V5 control**. It ran on the actual Windows VST3 and failed precisely as intended. No production image, Creative file or previous owner artifact was mutated.

## 6. Final separated gate verdicts

| Mandatory gate | Verdict |
|---|---|
| Engineering mandatory reading | **PASS** for enumerated current engineering documents |
| Production source review coverage | **PARTIAL** — transitive upstream DSP and every test assertion not fully inspected |
| Source defect screening | **FAIL** — F-01 and F-02 independently substantiated, further high-risk findings |
| Existing Win64 Steinberg validator | **PASS 47/47** on exact production baseline |
| Original ROM/SBK/77 SF2 private positive audio | **PASS for tested notes, presets and rates**; not blanket fidelity |
| New independent audit host negative | **FAIL as desired** — reproduces real V5 RPN loss |
| VST3 actual FX type GUI + audible effect | **FAIL GUI**, **NOT RUN PCM** |
| Same-instance realtime state/asset stress | **NOT RUN** |
| 125A Plugin Tester full gate | **NOT RUN** |
| Fender Studio Pro 8 live instrument GUI/recall | **NOT RUN** |
| Source license and release packaging | **NOT VERIFIED / RELEASE BLOCKER** |
| Second independent reviewer | **NOT RUN** |
| **FULL AUDIT / FULL QA PASS / RELEASE READY** | **NO** |
| **Product release recommendation** | **NO-GO** |

## 7. Consolidated corrective acceptance conditions (not a new numbered test series)

Before production promotion, fix V5 RPN/controller restore, re-enable and actually audition both FX TYPE menus, verify original-chip 0%-send semantics per approved spec, make getState/setState/audio ownership coherent, verify real GUI SF2 external path/asset identity through DAW save/reopen and actual PCM, repair browser-CC0 same-bank reconciliation and any teardown leaks, then run bounded overflow/NoteOff, parameter+audio automation (all 12 controls, real FX), concurrency/deadline and key sample rates, 125A Plugin Tester and one final Studio Pro 8 acceptance. Set correct final bundle name/version and settle public-distribution license obligations; keep Creative files private.

**No production change was made by this audit. The fact that the previous one-click Windows private test reported overall PASS is preserved, not retroactively falsified. Its legitimate tested scope and its blind spots are documented separately. The word FULL is intentionally NOT used for completion.**
