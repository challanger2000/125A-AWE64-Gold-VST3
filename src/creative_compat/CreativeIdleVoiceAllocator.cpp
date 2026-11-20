#include "CreativeIdleVoiceAllocator.h"
namespace awe64::creative {
std::optional<std::uint8_t> candidateOriginalWin95IdleMonoVoice(
    const std::array<OriginalVxdIdleVoiceProbe,kOriginalCreativeMusicVoices>& probes,
    const std::array<bool,kOriginalCreativeMusicVoices>& busy,
    const std::size_t usable) noexcept {
    if(usable==0 || usable>kOriginalCreativeMusicVoices)return std::nullopt;
    // The owner's installed original VXD LCOD has 3 outer iterations with
    // 32-byte-spaced voice structures and an inner voice index stride of 3.
    // Its idle branch checks marker 0x0FFE and a *read* high-word of zero.
    // We do not reproduce any VXD code bytes or guess busy-voice scores.
    for(std::size_t lane=0;lane<3;++lane) {
        for(std::size_t voice=lane;voice<usable;voice+=3) {
            const auto& p=probes[voice];
            if(!busy[voice] && p.driver_eligible &&
               p.driver_key_marker==0x0ffe &&
               p.chip_voice_target_high16==0) {
                return static_cast<std::uint8_t>(voice);
            }
        }
    }
    return std::nullopt;
}
} // namespace awe64::creative
