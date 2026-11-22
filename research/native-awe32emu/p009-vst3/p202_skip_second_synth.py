#!/usr/bin/env python3
"""P202: skip discarded SECOND synth at 44.1k; 86Box EMU8000 DSP unchanged."""
from pathlib import Path
import sys
def main():
    if len(sys.argv)!=2:
        raise SystemExit("usage: p202_skip_second_synth.py <pinned checkout>")
    path=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"/"Emu8000.cpp"
    s=path.read_text(encoding="utf-8")
    if s.count("P021_BOX86_ORIGINAL_CHIP_RESAMPLE")!=1:
        raise RuntimeError("P202 requires the pinned P021 actual chip path")
    if "P202_SKIP_DISCARDED_SECOND_SYNTH" in s:
        raise RuntimeError("P202 already patched")
    old="""    if (m_outputRate == kNativeSampleRate)
    {
        m_nativeL.resize(numFrames);"""
    if s.count(old)!=1:
        raise RuntimeError("P202 native 44100 original branch drift")
    new="""    // P202_SKIP_DISCARDED_SECOND_SYNTH:
    // Native 44.1k Box86 used to run both RenderNative's ENTIRE separate
    // 32-voice/chorus/reverb DSP then m_box.RenderFrame, discarding the first
    // synthesizer's PCM. Only the ORIGINAL 86Box sound is audible.
    // Keep the two trace/shadow clocks that RenderNative advanced.
    // Compile the old path only in the independent test reference binary.
#ifndef P202_CPU_LEGACY_REFERENCE
    if (m_chip == Chip::Box86 && m_outputRate == kNativeSampleRate)
    {
        for (uint32_t i = 0; i < numFrames; ++i)
        {
            int32_t l = 0, r = 0;
            m_box.RenderFrame(l, r);
            out[i * 2 + 0] = static_cast<int16_t>(
                std::clamp(l, int32_t{-32768}, int32_t{32767}));
            out[i * 2 + 1] = static_cast<int16_t>(
                std::clamp(r, int32_t{-32768}, int32_t{32767}));
        }
        m_waveCounter += numFrames;
        m_traceFrames += numFrames;
        return;
    }
#endif
    if (m_outputRate == kNativeSampleRate)
    {
        m_nativeL.resize(numFrames);"""
    path.write_text(s.replace(old,new),encoding="utf-8")
    print("P202_NATIVE_44100_AUTHENTIC_86BOX_DISCARDED_SOFTWARE_DSP_REMOVED")
if __name__=="__main__":main()
