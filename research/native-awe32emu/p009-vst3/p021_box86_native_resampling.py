#!/usr/bin/env python3
"""P021: Route ALL VST3 sample rates through the pinned authentic 86Box EMU8000.

The upstream Emu8000Core::RenderBlock() formerly used RenderNative()
(the alternative non-86Box synth!) for output rates !=44100 Hz. This could
produce a perfectly audible 48/96k VST3 yet never use the actual chip PCM.
Fix only the Box86 branch, before the alternative renderer is reached.
Original 44100 Hz path and original 86Box chip/filter/effects math stay intact.
Fail closed unless the pinned upstream body has the reviewed anchor.
"""
from pathlib import Path
import sys

def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: p021_box86_native_resampling.py <pinned checkout>")
    path=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"/"Emu8000.cpp"
    s=path.read_text(encoding="utf-8")
    anchor="""void Emu8000Core::RenderBlock(int16_t* out, uint32_t numFrames)
{
    if (numFrames == 0) return;
"""
    assert s.count(anchor)==1, "P021 upstream original RenderBlock changed"
    assert "P021_BOX86_ORIGINAL_CHIP_RESAMPLE" not in s, "P021 already applied"
    new=anchor+"""
    // P021_BOX86_ORIGINAL_CHIP_RESAMPLE:
    // At 48/96/192/8k the old resampler called RenderNative() rather than
    // m_box.RenderFrame(): it played the SECOND, approximate DSP core.
    // Now ALWAYS draw real 86Box EMU8000 stereo samples at 44100 native Hz,
    // then resample only those original int16 samples to the host clock.
    // No channel EQ, filter, voice, chorus or reverb algorithm is replaced.
    // The original exact 44100 Box86 path below remains fully untouched.
    if (m_chip == Chip::Box86 && m_outputRate != kNativeSampleRate)
    {
        const double ratio = static_cast<double>(kNativeSampleRate) /
                             static_cast<double>(m_outputRate);
        const size_t needed = static_cast<size_t>(
            std::ceil(m_resamplePos + ratio * numFrames)) + 2u;
        if (m_nativeL.size() < needed)
        {
            const size_t have = m_nativeL.size();
            m_nativeL.resize(needed);
            m_nativeR.resize(needed);
            for (size_t i = have; i < needed; ++i)
            {
                int32_t l = 0, r = 0;
                m_box.RenderFrame(l, r);
                m_nativeL[i] = static_cast<float>(
                    std::clamp(l, int32_t{-32768}, int32_t{32767}));
                m_nativeR[i] = static_cast<float>(
                    std::clamp(r, int32_t{-32768}, int32_t{32767}));
            }
        }
        for (uint32_t i = 0; i < numFrames; ++i)
        {
            const double q = m_resamplePos + ratio * i;
            const size_t i0 = static_cast<size_t>(q);
            const float f = static_cast<float>(q - i0);
            const float l = m_nativeL[i0] +
                (m_nativeL[i0 + 1] - m_nativeL[i0]) * f;
            const float r = m_nativeR[i0] +
                (m_nativeR[i0 + 1] - m_nativeR[i0]) * f;
            out[i * 2] = static_cast<int16_t>(std::clamp(
                std::lround(static_cast<double>(l)), -32768L, 32767L));
            out[i * 2 + 1] = static_cast<int16_t>(std::clamp(
                std::lround(static_cast<double>(r)), -32768L, 32767L));
        }
        m_resamplePos += ratio * numFrames;
        const size_t consumed = static_cast<size_t>(m_resamplePos);
        if (consumed > 0)
        {
            m_nativeL.erase(m_nativeL.begin(), m_nativeL.begin() + consumed);
            m_nativeR.erase(m_nativeR.begin(), m_nativeR.begin() + consumed);
            m_resamplePos -= static_cast<double>(consumed);
        }
        return;
    }
"""
    path.write_text(s.replace(anchor,new),encoding="utf-8")
    print("P021_48_96_192K_AUTHENTIC_BOX86_CHIP_OUTPUT_RESAMPLER_PATCHED")

if __name__=="__main__":
    main()
