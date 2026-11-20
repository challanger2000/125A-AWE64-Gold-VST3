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
        // P007BV: The original Creative Win95 guest now installs its
        // ROM-backed SYNTHGM.SBK in independent user bank2. User-SF2 bank1
        // remains available. The original drum preset MUST be selected
        // via Creative's actual CC0=2, CC32=0 and Program0 on CH10.
        // P007BU's CC0-only return left the previous SF2 kit active.
        if(bank_==126u) {
            if(stage_>=3u){committed_=true;return false;}
            const std::uint8_t status=stage_==2u?0xc9u:0xb9u;
            const std::uint8_t d1=stage_==1u?32u:0u;
            const std::uint8_t d2=stage_==0u?2u:0u;
            if(send(status,d1,d2)!=1)return false;
            ++stage_;return true;
        }
        // Host-bank tokens 126/127 identify original GM ROM or
        // real SF2 drum kits. All actual kit MIDI uses channel 10.
        const bool drums=(bank_==126u||bank_==127u);
        const auto wire=drums
            ?sf2_bank::route(bank_==127u?1u:0u,
                             sf2Loaded_ && bank_==127u)
            :sf2_bank::route(bank_,sf2Loaded_);
        const std::uint8_t cc=drums?0xb9u:0xb0u;
        const std::uint8_t pc=drums?0xc9u:0xc0u;
        std::uint8_t status=0u,d1=0u,d2=0u;
        switch(stage_) {
            case 0u: status=cc;d1=0u;d2=wire.cc0;break;
            case 1u: status=cc;d1=32u;d2=wire.cc32;break;
            case 2u: status=pc;d1=program_;d2=0u;break;
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
