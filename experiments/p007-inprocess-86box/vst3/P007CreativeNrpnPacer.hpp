#pragma once
// P007AE: original Win95 guest reads MIDI via a real emulated 16550 UART.
// Its receiver FIFO is bounded (16 bytes). A Creative NRPN consists of
// FOUR 6-byte P007 packets: sending 24 bytes in one audio callback can
// overrun the Win95 receiver before it polls. PACE: ONE 6-byte packet
// per callback (~23ms at 44.1 kHz / 1024 host block), and retry on reject.
//
// The host's p007_midi_submit() returning 1 means ENQUEUED, not proof of
// Win95 midiOutShortMsg() / the original Creative driver applying it.
// All state is owned exclusively by the VST3 audio callback. No heap/locks.
#include "P007CreativeNrpn.hpp"
#include <array>
#include <atomic>
#include <cstdint>

namespace p007::creative_nrpn {
// Unlike host block-size pacing, real elapsed-audio-time pacing also
// works with a 64/128-sample Studio Pro/ASIO buffer.
class FramePace final {
    std::uint32_t minGapFrames_=882u;
    std::uint64_t elapsedFrames_=0u;
    bool started_=false;
public:
    void configure(double sampleRate) noexcept {
        if(!(sampleRate>=8000.0 && sampleRate<=192000.0))
            sampleRate=44100.0;
        minGapFrames_=static_cast<std::uint32_t>(sampleRate*.020 + .5);
        reset();
    }
    void reset() noexcept {elapsedFrames_=0u;started_=false;}
    [[nodiscard]] bool due(std::uint32_t blockFrames) noexcept {
        if(blockFrames==0u)return false;
        if(!started_){started_=true;return true;}
        // One elapsed host block is accounted between successive calls.
        // Thus 64 frames @96k can NEVER send four MIDI messages within
        // 4*0.667ms merely because the DAW callback size is small.
        elapsedFrames_+=blockFrames;
        if(elapsedFrames_<minGapFrames_)return false;
        elapsedFrames_=0u;
        return true;
    }
};
class Pacer final {
    unsigned current_=0u,stage_=0u,next_=0u;
    std::uint16_t target_=0u;
public:
    void reset() noexcept {
        current_=stage_=next_=0u;
        target_=0u;
    }
    [[nodiscard]] unsigned stage() const noexcept{return stage_;}
    [[nodiscard]] unsigned selected() const noexcept{return current_;}
    // Cancel a partly received NRPN selector/data stream if guest resets.
    // Re-queue its full transaction for a later complete replay.
    void abort(std::atomic<std::uint32_t>& pending) noexcept {
        if(stage_!=0u)pending.fetch_or(1u<<current_,std::memory_order_acq_rel);
        stage_=0u;
    }
    template<class Sender>
    bool tick(std::atomic<std::uint32_t>& pending,
              const std::array<std::atomic<std::uint16_t>,count>& values,
              Sender&& send) noexcept {
        if(stage_==0u) {
            std::uint32_t bits=pending.exchange(0u,std::memory_order_acq_rel);
            if(!bits)return false;
            unsigned chosen=next_;
            for(unsigned offset=0u;offset<count;++offset) {
                const unsigned i=(next_+offset)%count;
                if(bits & (1u<<i)){chosen=i;break;}
            }
            const std::uint32_t mask=1u<<chosen;
            bits&=~mask;
            if(bits)pending.fetch_or(bits,std::memory_order_acq_rel);
            next_=(chosen+1u)%count;
            current_=chosen;
            target_=values[chosen].load(std::memory_order_acquire);
            if(!shouldTransmit(chosen,target_))return false;
            stage_=1u;
        }
        const auto cc=encode(current_,target_)[stage_-1u];
        if(!send(cc.number,cc.value))
            return true; // still active: retry exact packet later
        if(++stage_==5u) {
            stage_=0u;
            // Do not lose an edit made during this 4-block transaction.
            if(values[current_].load(std::memory_order_acquire)!=target_)
                pending.fetch_or(1u<<current_,std::memory_order_acq_rel);
        }
        return true;
    }
};
} // namespace p007::creative_nrpn
