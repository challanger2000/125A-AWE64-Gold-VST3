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
