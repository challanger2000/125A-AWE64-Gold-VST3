#pragma once
// P007AZ: original Win95 Creative BankSelect must not be sent as one 18-byte
// burst into the 16-byte emulated COM2 receive FIFO. One six-byte host wire
// packet per >=20 ms; settle once more before allowing MIDI notes.
// This ONLY schedules original MIDI packets, never replaces the synthesizer.
#include "P007Sf2BankRoute.hpp"
#include <cstdint>

namespace p007 {
class BankProgramPacer final {
    std::uint8_t bank_=0u,program_=0u;
    bool sf2Loaded_=false, requested_=false,committed_=false;
    unsigned stage_=0u; // CC0, CC32, program, settling, committed
public:
    void reset() noexcept {
        bank_=program_=0u;
        sf2Loaded_=requested_=committed_=false;
        stage_=0u;
    }
    [[nodiscard]] bool committed() const noexcept {return committed_;}
    [[nodiscard]] unsigned stage() const noexcept {return stage_;}
    void request(std::uint8_t bank,std::uint8_t program,
                 bool sf2Loaded,bool force=false) noexcept {
        if(!requested_||force||bank!=bank_||program!=program_||
           sf2Loaded!=sf2Loaded_) {
            bank_=bank;
            program_=program;
            sf2Loaded_=sf2Loaded;
            requested_=true;
            committed_=false;
            stage_=0u;
        }
    }
    template<class Sender> bool tick(Sender&& send) noexcept {
        if(!requested_||committed_)return false;
        const auto wire=sf2_bank::route(bank_,sf2Loaded_);
        std::uint8_t status=0u,d1=0u,d2=0u;
        switch(stage_) {
            case 0u: status=0xb0u;d1=0u;d2=wire.cc0;break;
            case 1u: status=0xb0u;d1=32u;d2=wire.cc32;break;
            case 2u: status=0xc0u;d1=program_;d2=0u;break;
            default:
                // One more >=20 ms interval after accepted ProgramChange
                // before MIDI NoteOn or a queued NRPN can follow it.
                committed_=true;
                return false;
        }
        if(send(status,d1,d2)!=1)return false; // do not skip rejected packets
        ++stage_;
        return true;
    }
};
} // namespace p007
