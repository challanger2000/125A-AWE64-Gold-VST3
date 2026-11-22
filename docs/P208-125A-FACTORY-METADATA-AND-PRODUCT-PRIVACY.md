# P208 — Standard 125A brand and private identity in actual VST3 metadata

**Problem:** the prior prototype used `AWE64_Gold_Native_P009`
in distributable `Contents/Resources/moduleinfo.json` and embedded
a developer's personal GitHub profile link in both plugin factory
binary strings and `moduleinfo.json`. The user's 125A brand and
privacy rule forbids personal account names in any public-facing
product metadata or download.

**Fix:** `moduleinfo.strict.json` now has canonical
`Name: 125A-AWE64-Gold`. Vendor remains `125A Audio`, the known
existing vendor string; factory URL is intentionally empty until an
**official, neutral 125A URL** exists. `P009Factory.cpp` also
exposes an empty factory URL so no personal account namespace is
embedded in a Windows VST3 binary. Preset names, class display
name `125A AWE64 Gold`, **same 16-byte Steinberg class ID**,
existing VST3 processor GUI/automation/project-state identifiers
and Creative/EMU8000 original DSP remain unchanged.

**Windows native executable evidence gate:** inspect the actual
compiled `Contents/x86_64-win/*.vst3` bytes, its actual installed
`moduleinfo.json`, validate the precise vendor, FUID, class and
display names, and fail if the previous personal account string is
present in either. Run this gate **before** Steinberg validator
and independent host/125A testers; no cosmetic placebo-only source
check. The internal CI CMake target path still names the development
target to keep existing Windows regression tooling stable. A
user-facing private package must use the canonical
`125A-AWE64-Gold.vst3` bundle name (as the prior P192 archive did).

Not a claim that final licensed asset distribution or owner Win95
hardware/DAW acceptance is finished; source privacy and final release
approval are separately gated.

## Completed P208 Windows x64 native VST3 acceptance

- **Exact unchanged actual product-code SHA:** `e251c155ac74478b2810989bc3bc2e40d0931afe`.
- **Subsequent QA-only fix:** `58cff604d349217bd68cabcae62a992737679381`, correcting the *previously existing* strict-moduleinfo expectation to the deliberately changed canonical plugin name. No C++ implementation, factory or module JSON modification in QA-only commit.
- **Complete full regression:** [Windows Actions #45066926832](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/45066926832) **COMPLETED SUCCESS**, job `137345053588`, **47 successfully executed workflow steps**.
- Actual **Windows DLL bytes** and actual `Contents/Resources/moduleinfo.json` scanned, no personal account text in either; canonical `125A-AWE64-Gold`, unchanged 125A vendor, unchanged original VST3 FUID, correct display name. P208 explicit production-binary privacy oracle **PASS**.
- Steinberg VST3 Validator **47 PASS / 0 FAILED**, source/factory/JSON strict metadata **PASS**.
- Published 125A Plugin Tester v0.2.10 **41 PASS / 0 WARNING / 0 FAIL**; v0.3.3 **41 PASS / 0 WARNING / 0 FAIL**.
- Actual original 86Box chip P207 three fixtures x live/before-note pitch and deliberately broken old-value mutant **PASS**, with original negative IP `65535` versus old mistaken `0`. P204/P205/P206 source/real-chip regression and deliberate mutators **PASS**.
- Sounding original-chip host state P187 **1,120,803 accepted / 0 stale / 0 mixed / 0 rejected**, nonzero genuine-chip PCM energy `1280.720460930`. Original chip CPU p95/PCM A/B, two VST3 EditorHost views, actual Win64 interrupted installer **BACKUP** and **TARGET** fail-closed integrity tests **PASS**.

### Separate private, offline package-integrity staging (NOT A RELEASE)

The Windows-produced P208 compiled native DLL was combined LOCALLY with
the owner-provided original `awe32.raw`, `SYNTHGM.SBK`,
`Synth 512 Bank.sf2` (77 exact presets, program 63 absent) and
`Vintage Dreams Bank.sf2`. The P192 canonical
`125A-AWE64-Gold.vst3` private installer code was retained. This
staging archive **does not** contain three unrelated P204 synthetic
SF2 fixtures found in the diagnostic CI bundle and **does not**
contain a dev-named VST3 product bundle. No owner proprietary bytes
were uploaded to GitHub.

Local private staging integrity: **18 files**, all SHA256 entries,
ZIP CRC, unique/canonical member paths and module identity validated.
Archive SHA256:
`c416430edcaca9cd4e83394ef2312390894c5fbe547cdb55a1eee1af69c9d46f`.
Staged actual VST3 module SHA256:
`a78006eed0a8a571c2c4410b770ef9f95b9f2d0adf97734f47c6eec843ef9664`.
Owner's 13 previously provided SF2 archives passed complete CRC
verification LOCALLY; extra banks are not silently merged or
presented as part of default 77-preset SF2. Internal private staged
package was **not sent to user or published**.

**Remaining release NO-GO / not tested:** authentic owner original
Creative Win95 guest vs product register and PCM parity across the
private ROM/SBK and 13 banks; actual installed Windows Studio Pro
ASIO CPU, latency and playback; independent complete Codex release
audit, private installer integration check against this particular
new staging ZIP, and exact ownership/redistribution licensing.
Compiled VST3 QA and static private ZIP integrity alone are NOT
a complete final release acceptance.
