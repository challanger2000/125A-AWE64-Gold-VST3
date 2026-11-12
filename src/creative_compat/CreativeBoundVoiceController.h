#pragma once
// P002 stateful ORIGINAL Creative ROM note/controller bridge.
// Physical voices and ENCODED instrument terms are explicit inputs until
// original Win95 allocation and generator conversions have been proven.
#include "CreativeControllerVolume.h"
#include "CreativePitchCandidate.h"
#include "CreativeIdleVoiceAllocator.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace awe64::creative {
enum class BoundVoiceActionKind : std::uint8_t {
    UpdateIfatn, UpdatePitch, RequestEnvelopeRelease, RequestImmediateKill
};
struct BoundVoiceAction {
    BoundVoiceActionKind kind=BoundVoiceActionKind::UpdateIfatn;
    std::uint8_t physical_voice=0;
    std::array<PortWord,2> writes{};
    // Writes are valid only for UpdateIfatn / UpdatePitch.
    // Release and kill are semantic requests, NOT fabricated chip registers.
};
struct BoundVoiceActions {
    // CC121 may reset expression and release pedal-held voices together.
    std::array<BoundVoiceAction,60> actions{};
    std::size_t count=0;
};
struct OriginalRomInitialRegisterCandidates {
    std::array<PortWord,2> pitch{};
    std::array<PortWord,2> filter_and_attenuation{};
    // Pair ordering and exact chip-frame scheduling remain UNVERIFIED.
};
struct OriginalRomAllocatedRegisterCandidates {
    std::uint8_t physical_voice=0;
    OriginalRomInitialRegisterCandidates initial{};
};
class CreativeBoundVoiceController {
public:
    // Bind a caller-allocated EMU8000 voice to exactly one original ROM
    // note layer. The original physical voice allocator is NOT implemented.
    [[nodiscard]] std::optional<OriginalRomInitialRegisterCandidates> bind(
        const CreativeRomNotePlan& plan, std::size_t layer_index,
        std::uint8_t physical_voice, const OriginalCreativeIfatnTerms& encoded_terms) noexcept;
    // The original VXD's exact idle MONO search is available when the caller
    // supplies its observed driver-key and chip-target state for 30 voices.
    // Busy-envelope priority and stealing are NOT guessed.
    [[nodiscard]] std::optional<OriginalRomAllocatedRegisterCandidates>
    bindFirstOriginalIdleMono(
        const CreativeRomNotePlan& plan, std::size_t layer_index,
        const OriginalCreativeIfatnTerms& encoded_terms,
        const std::array<OriginalVxdIdleVoiceProbe,30>& original_probe,
        std::size_t available_voices=30) noexcept;
    // Controller CC7/CC11 -> IFATN, pitchbend -> IP, NoteOff/CC64/CC120/
    // CC121/CC123 -> semantic release/kill requests.
    [[nodiscard]] std::optional<BoundVoiceActions> process(
        const MidiMessage& message) noexcept;
    // Notify after the original chip actually finishes the release/steal;
    // no timer, age or voice-stealing heuristic is silently assumed.
    bool clearPhysicalVoice(std::uint8_t physical_voice) noexcept;
    [[nodiscard]] std::size_t occupiedVoiceCount() const noexcept;
    [[nodiscard]] const MidiState& midi() const noexcept { return state_; }
private:
    struct Voice {
        bool occupied=false;
        bool key_down=false;
        bool release_requested=false;
        std::uint8_t channel=0,key=0,velocity=0;
        SbkLayer layer{};
        OriginalCreativeIfatnTerms terms{};
    };
    MidiState state_{};
    std::array<Voice,30> voices_{};
};
} // namespace awe64::creative
