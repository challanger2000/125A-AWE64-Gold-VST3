#!/usr/bin/env python3
"""P198 isolated original Creative SF2 filter Fc/Q driver-unit conversion.

Private reference: original owner SBAWE.VXD 4.38.0.2, LCOD conversion
and merge verified against SHA256 d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a.

The original SF2 driver converts INSTRUMENT and PRESET generators
independently into internal signed 16-bit descriptor units, then adds
the results and clamps the final hardware value. Filters are NONLINEAR:
  instrument Fc = trunc_toward_zero((raw_cents+15)/29)-153
  missing instrument Fc -> original template 313
  preset Fc offset = same_converter(preset)-same_converter(0)
  instrument Q = trunc_toward_zero(raw_cB/12)
  preset Q offset = trunc_toward_zero(raw_cB/12)
  hardware Fc = clamp(instrument Fc + preset delta, 0..255)
  hardware Q = clamp(instrument Q + preset delta, 0..15)
Raw-SF2 addition before converting is not equivalent due to quantization.
Original source instead ignores one side (FillFrom) and scales Q by /16.
This patch does NOT touch SF1/SBK, ROM, original EMU8000 or VST3 state.
"""
from pathlib import Path
import sys

def one(original,old,new,name):
    n=original.count(old)
    if n!=1:
        raise RuntimeError(f"P198 pinned {name} anchor mismatch (count={n})")
    return original.replace(old,new)

def main():
    if len(sys.argv)!=2:
        raise SystemExit("usage: p198_creative_sf2_filter_converted_scope.py <pinned upstream checkout>")
    root=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"
    header=root/"SoundFont.h"
    source=root/"SoundFont.cpp"
    h=header.read_text(encoding="utf-8")
    c=source.read_text(encoding="utf-8")
    if "P198 original Creative SF2" in h or "P198 original Creative SF2" in c:
        raise RuntimeError("P198 applied twice")
    h=one(h,"        int sf1AttenUnits = -1;",
"""        int sf1AttenUnits = -1;
        // P198 original Creative SF2 nonlinear filter conversions happen
        // independently at instrument and preset scope BEFORE descriptor sum.
        // Negative means a manually constructed region has no prepared value.
        int sf2FilterFcReg = -1;
        int sf2FilterQReg = -1;""","region")
    c=one(c,"            r.gen.FillFrom(presetGen);",
"""            if (version == Version::Sf2)
            {
                // P198: original Creative generator import code converts
                // each scope before its saturating signed-word merge. The
                // default instrument filter cutoff in SBAWE.VXD is 313,
                // intentionally higher than the 255 hardware maximum.
                // C++ integer division on signed integers truncates to zero.
                const auto fcToDriverUnits = [](int x) -> int {
                    return (x + 15) / 29 - 153;
                };
                const int instrumentFc =
                    r.gen.Has(Gen::InitialFilterFc)
                        ? fcToDriverUnits(r.gen.Get(Gen::InitialFilterFc, 0))
                        : 313;
                const int presetFcDelta =
                    presetGen.Has(Gen::InitialFilterFc)
                        ? fcToDriverUnits(presetGen.Get(Gen::InitialFilterFc, 0))
                          - fcToDriverUnits(0)
                        : 0;
                r.sf2FilterFcReg =
                    std::clamp(instrumentFc + presetFcDelta, 0, 255);
                const int instrumentQ = r.gen.Get(Gen::InitialFilterQ, 0) / 12;
                const int presetQ = presetGen.Get(Gen::InitialFilterQ, 0) / 12;
                r.sf2FilterQReg =
                    std::clamp(instrumentQ + presetQ, 0, 15);
            }
            // Keep SF1 observed FillFrom and the P195 restricted linear
            // SF2 merging unchanged. Only filter register output uses the
            // two independently converted/summed original-driver fields.
            r.gen.FillFrom(presetGen);""","select")
    c=one(c,
"""    if (sf1) q = g.Get(Gen::InitialFilterQ, 0) >> 3;
    else     q = static_cast<int>(std::lround(g.Get(Gen::InitialFilterQ, 0) / 10.0
                                              / kResonanceMaxDb * kCccaQMax));""",
"""    if (sf1) q = g.Get(Gen::InitialFilterQ, 0) >> 3;
    else if (region.sf2FilterQReg >= 0) q = region.sf2FilterQReg;
    else q = static_cast<int>(std::lround(g.Get(Gen::InitialFilterQ, 0)
                                          / 10.0 / kResonanceMaxDb * kCccaQMax));""","Q")
    c=one(c,
"""    int cutoff;
    if (!g.Has(Gen::InitialFilterFc)) cutoff = 255;
    else if (sf1)                     cutoff = std::clamp<int>(
                                          g.value[Gen::InitialFilterFc] * 2, 0, 255);
    else                              cutoff = FilterFcFromAbsCents(g.value[Gen::InitialFilterFc]);""",
"""    int cutoff;
    if (sf1) {
        if (!g.Has(Gen::InitialFilterFc)) cutoff = 255;
        else cutoff = std::clamp<int>(g.value[Gen::InitialFilterFc] * 2, 0, 255);
    }
    else if (region.sf2FilterFcReg >= 0) cutoff = region.sf2FilterFcReg;
    else cutoff = g.Has(Gen::InitialFilterFc)
        ? FilterFcFromAbsCents(g.value[Gen::InitialFilterFc]) : 255;""","cutoff")
    header.write_text(h,encoding="utf-8")
    source.write_text(c,encoding="utf-8")
    print("P198_CREATIVE_SF2_NONLINEAR_FILTER_SCOPE_CONVERSION_SF1_UNCHANGED_PATCHED")

if __name__=="__main__":
    main()
