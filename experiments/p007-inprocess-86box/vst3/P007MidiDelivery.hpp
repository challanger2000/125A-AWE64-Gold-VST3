#pragma once
// P007AQ: bounded, allocation-free original Creative WinMM note delivery.
// p007_midi_submit returning 0 means NOT queued. Dropping such a NoteOff
// can leave a real guest voice stuck indefinitely. The VST3 audio callback
// owns this state exclusively; it never touches the guest CPU or blocks.
//
// The queue preserves NoteOn/NoteOff ordering AFTER the first rejection.
// It does not pretend to give sample-accurate Win95/UART timestamps.
// On overflow, stop accepting notes and deliver three STANDARD MIDI CCs per
// channel before allowing new messages: CC64=0 (release sustain pedal),
// CC120=0 (All Sound Off), then CC123=0 (All Notes Off). Original Creative
// driver behavior remains empirical, not assumed from MIDI-spec compliance.
#include <array>
#include <cstdint>
namespace p007 {
class MidiDelivery final {
public:
    struct Message {
        std::uint8_t status{},data1{},data2{};
    };
    static constexpr unsigned capacity=256u;
    void reset() noexcept {
        head_=0u;size_=0u;panicPacket_=48u;
    }
    // Explicit host-iterator overflow: release original Creative voices
    // rather than accepting an unknown tail of NoteOn/NoteOff messages.
    // Shares the existing FIFO-overflow panic sequence (3 CCs x 16 ch).
    void requestRecovery() noexcept {
        head_=size_=0u;
        panicPacket_=0u;
    }
    [[nodiscard]] unsigned pending() const noexcept {return size_;}
    [[nodiscard]] bool panicPending() const noexcept {
        return panicPacket_<48u;
    }
    // Called only by the DAW audio consumer thread.
    template<class Send>
    bool submit(std::uint8_t status,std::uint8_t d1,
                std::uint8_t d2,Send&& send) noexcept {
        if(panicPending())return false;
        if(size_==0u && send(status,d1,d2)==1)return true;
        if(size_==capacity) {
            // Never let newer MIDI overtake the missing NoteOff. Instead
            // After a lost NoteOff, stuck sustain is possible: CC123 alone
            // does not always release a note held by pedal CC64=127.
            // Reset sustain, silence the voices, then All Notes Off on
            // every channel, over the SAME original Creative UART.
            requestRecovery();
            return false;
        }
        queue_[(head_+size_)%capacity]={status,d1,d2};
        ++size_;
        return true; // accepted by bounded retry queue, NOT yet by Win95.
    }
    template<class Send>
    unsigned drain(Send&& send,unsigned budget=32u) noexcept {
        if(panicPending()) {
            if(budget==0u)return 0u;
            const auto channel=panicPacket_/3u;
            const auto stage=panicPacket_%3u;
            const auto status=static_cast<std::uint8_t>(0xb0u|channel);
            const auto controller=static_cast<std::uint8_t>(
                stage==0u?64u:(stage==1u?120u:123u));
            if(send(status,controller,0u)!=1)return 0u;
            ++panicPacket_;
            return 1u; // one panic CC per callback; no UART flood
        }
        unsigned delivered=0u;
        while(size_>0u && delivered<budget) {
            const Message msg=queue_[head_];
            if(send(msg.status,msg.data1,msg.data2)!=1)break;
            head_=(head_+1u)%capacity;
            --size_;
            ++delivered;
        }
        return delivered;
    }
private:
    std::array<Message,capacity> queue_{};
    unsigned head_=0u,size_=0u,panicPacket_=48u;
};
} // namespace p007
