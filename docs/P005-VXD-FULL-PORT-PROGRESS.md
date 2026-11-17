# P005 native Creative port: NOT COMPLETE

This change ports two additional original note-start branches and fixes
native validation. It does not activate an incomplete driver in the VST3.

## Implemented original branches

Source: owner-supplied `SBAWE.VXD` 4.38.0.2, SHA-256
`d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`.
Offsets below are relative to the LE **LCOD object**, not file offsets.

- `+0x56CE..+0x5753`: pitch accumulation uses wrapping 32-bit arithmetic,
  a signed upper clamp (`+0x573E`) and arithmetic shift (`+0x574C`). The
  C++ compiler now preserves negative and overflowing results as the
  original does, instead of rejecting negative pitch or saturating a
  non-wrapping 64-bit intermediate.
- `+0x5B81..+0x5BE4`: mode bit 0 selects loop addresses or the nonlooping
  `end+4`/`end+8` path. Nonlooping RAM samples are now accepted; unused
  loop addresses cannot reject that branch. Physical 24-bit pointer
  bounds still apply. Looping samples still validate both loop addresses.
- MIDI pitch bend rejects device capacities outside 1..32 before walking
  the fixed voice array or changing channel state. This is a native safety
  check, not a newly reconstructed driver algorithm.
- The Linux RAM-capture validation now uses OpenSSL EVP SHA-256 instead
  of unconditionally reporting SHA unavailable. Windows continues to use
  BCrypt. No provenance check is bypassed. OpenSSL Crypto development
  files are required only for the non-Windows native harness.

The original EMU8000/CT1745 sources and the already verified PCOD
`+0x3BD4` envelope conversion remain unchanged.

## Validation performed

- Root native Debug build with `AWE64_ENABLE_P005_ORIGINAL_CREATIVE=ON`:
  **85/85 CTests passed** (baseline: 84/85; RAM source SHA unavailable).
- Original-port GCC ASan/UBSan build: **32/32 CTests passed**, including
  the existing zero-heap MIDI tests.
- **4,096 synthetic descriptor cases executed against the actual private
  original x86 blocks** using Unicorn, then compared with the compiled
  C++23 register compiler: **0 mismatches**. The unchanged base compiler
  produces **2,860 mismatches** on the same corpus. All 128 MIDI keys,
  four sample modes, ROM/RAM geometry, signed pitch inputs and DWORD
  overflow are represented. Synthetic inputs are not Win95/VST3 audio
  acceptance, even though the oracle executes real original instructions.
- Private two-bank SMALW/SMLD comparison: the existing native uploader
  generated **523,182 / 131,457 port operations**, respectively, with
  **0 value/order mismatches** against the supplied original measurement.
  This checks upload address/data operations only, not scratch-voice
  reads, intervening MIDI, CPU timing, all initialization or native PCM.
  The newer measurement has different frame windows from the older
  private audit. Transfer starts were located by full SMALW addresses;
  the other initial bank-1-address occurrences were not complete matching
  bank transfers. Private package/measurement SHA bindings were verified.

Reproduce the native checks from the repository root:

```sh
cmake -S . -B /workspace/awe64-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DAWE64_ENABLE_P005_ORIGINAL_CREATIVE=ON
cmake --build /workspace/awe64-build --parallel 2
ctest --test-dir /workspace/awe64-build --output-on-failure --parallel 2
```

For the optional original-instruction differential, install `unicorn==2.1.4`
in a local Python environment and supply the owner driver outside Git:

```sh
python tools/p005_compare_original_note_geometry.py \
  --private-vxd /private/SBAWE.VXD \
  --compiled-probe /workspace/awe64-build/p005-original-creative-port/awe64_p005_original_note_geometry_probe
```

The probe contains no driver bytes or original lookup tables. The Python
runner rejects any driver with the wrong SHA and prints aggregate results
only. REA, Ghidra and IDA were not available in this environment; Capstone
5.0.6, the existing LE relocation parser and Unicorn were used.

## Remaining acceptance gates

- The bounded autonomous Format1/SF2 bank converter replacing the 16,384
  precompiled-zone catalog is not implemented. No limit was merely raised,
  and no key/velocity table expansion was introduced.
- Active SF2 modulator conversion remains unsupported. Its original
  loader/control-flow entry points still require reconstruction; no
  unverified SoundFont-standard substitute was added.
- The owned 288-slot catalog is not automatically and fully authenticated
  and activated for a complete user bank in the exported VST3.
- Autonomous x86-derived timing, complete pre-bank-2 state and full
  native register/frame/DRY-PCM parity remain unproven. The supplied
  measurement report itself marks CPU/frame calibration false.
- Windows x64/MSVC, the VST3 host and 125A Plugin Test Lab are unavailable
  in this Linux instance. No Windows build or plugin-host pass is claimed.

Private drivers, ROMs, SoundFonts, measurements, generated port dumps and
extracted code stay outside the repository and public artifacts.
