#pragma once
// P007AQ: bounded, allocation-free original Creative WinMM note delivery.
// p007_midi_submit returning 0 means NOT queued. Dropping such a NoteOff
// can leave a real guest voice stuck indefinitely. The VST3 audio callback
// owns this state exclusively; it never touches the guest CPU or blocks.
//
// The queue preserves NoteOn/NoteOff ordering AFTER the first rejection.
// It does not pretend to give sample-accurate Win95/UART timestamps.
// On overflow, stop accepting notes and deliver CC123 on all MIDI channels
// before allowing new messages. CC123 support is MIDI-standard; this is a
// fallback, not proof that original Creative always obeys it.
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
        head_=0u;size_=0u;panicChannel_=16u;
    }
    [[nodiscard]] unsigned pending() const noexcept {return size_;}
    [[nodiscard]] bool panicPending() const noexcept {
        return panicChannel_<16u;
    }
    // Called only by the DAW audio consumer thread.
    template<class Send>
    bool submit(std::uint8_t status,std::uint8_t d1,
                std::uint8_t d2,Send&& send) noexcept {
        if(panicPending())return false;
        if(size_==0u && send(status,d1,d2)==1)return true;
        if(size_==capacity) {
            // Never let newer MIDI overtake the missing NoteOff. Instead
            // force every channel to All Notes Off using the SAME UART.
            head_=size_=0u;panicChannel_=0u;
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
            const auto status=static_cast<std::uint8_t>(
                    0xb0u|panicChannel_);
            if(send(status,123u,0u)!=1)return 0u;
            ++panicChannel_;
            return 1u; // one panic CC per callback; no 16*6-byte UART burst
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
    unsigned head_=0u,size_=0u,panicChannel_=16u;
};
} // namespace p007
