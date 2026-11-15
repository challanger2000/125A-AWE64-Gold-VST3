# P003 – Independently captured ORIGINAL Creative Windows 95 two-bank source (15 Nov 2026)

This report records **actual owner-supplied, unmodified ORIGINAL Creative Win95
AWEMAN32.DLL/SBAWE.VXD execution** in the pinned, calibrated 86Box AWE32
fork. The proprietary Windows VM, ROM, Creative binaries, SF2 samples,
unabridged trace and WAV remain **private** and are NOT stored in Git.

## Source identity and scope

- Private source ZIP: `P003-20261115T125019Z-ca0863fd-ERGEBNIS.zip`
- ZIP SHA256: `badf6e05a7d4e06909a5fde706a70f8f4cb6c346f5737176e2a130d2b794ce78`
- Original physical EMU8000 trace SHA256:
  `181b2bc29a0a3118ea6eb8bc6d921b5447b2c9bf5714356b85d1176e4a6e9548`
- Original DRY 44.1 kHz stereo PCM16 WAV SHA256:
  `35315175420592f5424bbcd9f7d0d73a151fb49989f7b8ec69aa5138c99a8e12`
- Original guest `P002RAM.LOG` SHA256:
  `10a50fdb5b5faaff84b11e6c830e2399532a493ce1d41e6807fdd2787c178441`
- First SF2 SHA256:
  `d5b99ec38fddf59f53f66b90921e170d207bf3bbd73960f6e4175c39ef68349b`
- Second independently generated SF2 SHA256:
  `095488175e0535594dc47882193019e07656f47180d7eaaf2f6476e6c7fdaae8`
- Original 86Box fork EXE and ROM digests were confirmed by the owner-side
  session preflight. This report does **not** embed those assets.

## Verified physical RAM layout

Both banks were loaded by the **original 1996 Creative AWEManager API**;
the true 86Box EMU8000 SMALW/SMLD port writes, not API return values
alone, established the **physical** sample addresses.

| Source observation | Bank 1 | Bank 2 |
|---|---:|---:|
| First physical sample word address | `0x200010` | `0x23FB59` |
| Last physical sample word address | `0x23FB48` | `0x247B58` |
| Source SF2 sample words | 260,921 | 32,768 |
| Original driver AWE_QUERY_DRAM_SIZE consumed words | 260,937 | 32,784 |
| Per-bank allocation overhead | **16** | **16** |
| All final sample words versus observed three-tap/zero-run prediction | **260,921 equal; 0 differ** | **32,768 equal; 0 differ** |

The first source sample's end exclusive is `0x23FB49`.
The second source sample starts at **`0x23FB59`**, exactly **16 more
16-bit words** after the preceding pool's end. Naive first-fit with no
per-bank overhead incorrectly chose **`0x23FB49`**, now a proven
negative control. This is the key **P003 observed allocator bug**.

The source register stream shows an original 16-word zero preface and
15-word zero postface per upload, with the second bank's zero preface
overlapping its predecessor's zero postface. The authentic Creative
driver's reported memory consumption is still exactly source-word-count
plus 16 for **each** bank. The last RAM sample of bank 1 is not
overwritten by bank 2. Additional source RAM writes/repeats caused by
the original chip's 1024-word transfer chunking are kept separate from
the unique sample-word count; none creates a sample-data mismatch.

Original second-bank `AWE_CLEAR_USER_BANK` succeeded and
`AWE_QUERY_DRAM_SIZE` reported **14,419,127 free words** after its
release, exactly the value immediately **before** loading bank 2.

## Actual original dry audio

The original independent capture contains a non-silent second-bank
note at approximately **39.2–39.5 s** (RMS approximately **−29.38 dBFS**),
dominant spectral component approximately **231.5 Hz**, with the odd
harmonics expected for the generated square-wave comparison SF2.
Bank 1 has independently audible source notes; at 37.1–37.4 s RMS is
approximately **−22.24 dBFS**. These are different instruments:
**no gain, timbre or VST3 parity conclusion is inferred from comparing
their different note levels**.

## Implementation resulting from this genuine original capture

`CreativeCandidateRamAllocator.h` now explicitly reserves an
**observed 16-word gap per sample pool** in `allocateCreativeSampleWords`
and the normal `allocateMonoSf2` path; lease `.words` still denotes
the *actual playable sample-word count*, not overhead. Raw testing
`allocate` remains available for explicit unguarded memory contracts.
The test `p003_candidate_multi_bank_ram_allocator_contract` locks
the real source addresses `0x200010` and `0x23FB59`, including the
two original DRAM word deltas; the actual SF2 multi-bank CTest locks
the same guard behavior without leaking vendor/private sample media.

**Do not mark Creative allocator general equivalence from these
two banks.** Fragmentation, compaction, arbitrary SF2 sample layouts,
load/release orders, and persistent voice residency need distinct
source controls. The three-tap sample transform has now matched **two
different authentic banks** in full, but is still an empirically
observed mapping, not proof for all SF2 input classes.

**Native VST3-vs-actual Win95 dry PCM bit parity remains unverified**.
The original EMU8000 core was not changed. P003 is not a general
release GO.
