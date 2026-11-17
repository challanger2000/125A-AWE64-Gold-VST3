#pragma once
// P003 original Win95 Creative NRPN owner, non-RT only.
// Owns fixed-size verified-session source data; attaches non-owning views
// to the SAME MidiEngine that owns the live physical voice allocator.
// No Creative proprietary tables/ROM/SF2 bytes appear in this header.
// Source authenticity MUST be independently established by the caller.
#include "OriginalCreativeMidiEngine.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace awe64::original_vxd::research {
class OriginalCreativeNrpnOwnedSource final {
public:
    static constexpr std::size_t kDeviceBytes=0x1592u;
    static constexpr std::size_t kParameterBytes=30u*6u;
    static constexpr std::size_t kLookupWords=128u;

    OriginalCreativeNrpnOwnedSource() noexcept=default;
    OriginalCreativeNrpnOwnedSource(
        OriginalCreativeNrpnOwnedSource const&)=delete;
    OriginalCreativeNrpnOwnedSource& operator=(
        OriginalCreativeNrpnOwnedSource const&)=delete;
    OriginalCreativeNrpnOwnedSource(
        OriginalCreativeNrpnOwnedSource&&)=delete;
    OriginalCreativeNrpnOwnedSource& operator=(
        OriginalCreativeNrpnOwnedSource&&)=delete;

    [[nodiscard]] bool attached() const noexcept {return attached_!=nullptr;}

    // PRECONDITION: called only while host processing is fully stopped.
    // All THREE source spans must derive from the same owner-authenticated
    // Creative Win95 capture / SHA-verified driver revision. Flags are
    // caller declarations, NOT proof. No allocations or locks here.
    [[nodiscard]] bool attachNonRealtime(
        MidiEngine& midi,
        std::span<const std::uint8_t> originalDevice,
        std::span<const std::uint8_t> originalParameterTable,
        std::span<const std::uint16_t> originalLookup,
        bool independentOriginalDeviceAttestation,
        bool independentOriginalDriverTableAttestation,
        bool independentOriginalVoiceRowAttestation) noexcept {
        if(attached_!=nullptr || midi.original_nrpn_callbacks_installed_verified ||
           midi.original_reset_callback_absent_verified ||
           !midi.original_nrpn_memory.device.empty() ||
           !independentOriginalDeviceAttestation ||
           !independentOriginalDriverTableAttestation ||
           !independentOriginalVoiceRowAttestation ||
           originalDevice.size()!=kDeviceBytes ||
           originalParameterTable.size()!=kParameterBytes ||
           originalLookup.size()!=kLookupWords)return false;
        // Original imported device snapshot has the 125A capture footer.
        // The footer is format evidence, NOT a cryptographic attestation.
        const auto read16=[originalDevice](std::size_t at) noexcept {
            return nrpn_detail::r16(originalDevice,at);
        };
        const auto read32=[&read16](std::size_t at) noexcept {
            return static_cast<std::uint32_t>(read16(at))|
                   (static_cast<std::uint32_t>(read16(at+2u))<<16u);
        };
        if(read32(0x158eu)!=0x4d474d31u ||
           read16(6u)==0u ||
           read16(6u)>physical_voices ||
           read16(6u)!=midi.device.physical_capacity)
            return false;
        // The native voice allocator and original byte-image MUST refer
        // to the same physical rows. Never bind a stale guest snapshot.
        for(std::size_t v=0u;v<midi.device.physical_capacity;++v) {
            const auto at=0x42u+v*0x20u;
            auto const& live=midi.device.voices[v];
            if(read16(at)!=live.key_marker ||
               (live.original_pitch_base_verified &&
                read16(at+0x0eu)!=live.original_pitch_base))
                return false;
        }
        // Original mode 0x0000/0x0100/0x0200 only: unrecognized channel
        // selectors are not safely importable into this MIDI port.
        for(std::size_t ch=0u;ch<16u;++ch) {
            const auto row=0x24u*ch;
            const auto mode=read16(0x45eu+row);
            if(mode!=0u && mode!=0x0100u && mode!=0x0200u)
                return false;
        }
        // All validation before changing live state (transactional).
        for(std::size_t i=0u;i<kDeviceBytes;++i)
            device_[i]=originalDevice[i];
        for(std::size_t i=0u;i<kParameterBytes;++i)
            parameters_[i]=originalParameterTable[i];
        for(std::size_t i=0u;i<kLookupWords;++i)
            lookup_[i]=originalLookup[i];
        midi.original_nrpn_memory={device_,true};
        midi.original_nrpn_callbacks_installed_verified=true;
        midi.original_nrpn_parameter_callback_installed_verified=true;
        midi.original_nrpn_parameter_tables_verified=true;
        midi.original_nrpn_voice_rows_verified=true;
        midi.original_nrpn_parameter_entries=parameters_;
        midi.original_nrpn_parameter_lookup=lookup_;
        for(std::size_t ch=0u;ch<16u;++ch) {
            const auto at=0x24u*ch;
            auto& state=midi.channel[ch];
            state.original_rpn_mode=read16(0x45eu+at);
            state.original_rpn_number=read16(0x460u+at);
            state.original_rpn_aux_462=read16(0x462u+at);
            state.original_rpn_data_lsb=read16(0x464u+at);
            state.original_bend_delta=std::bit_cast<std::int32_t>(
                read32(0x442u+at+0x14u));
        }
        attached_=&midi;
        return true;
    }

    // PRECONDITION: VST3 audio callbacks are stopped and engine is not
    // processing MIDI. Detach before any owner lifetime ends.
    void detachNonRealtime() noexcept {
        if(attached_==nullptr)return;
        attached_->original_nrpn_memory={};
        attached_->original_nrpn_callbacks_installed_verified=false;
        attached_->original_nrpn_parameter_callback_installed_verified=false;
        attached_->original_nrpn_parameter_tables_verified=false;
        attached_->original_nrpn_voice_rows_verified=false;
        attached_->original_nrpn_parameter_entries={};
        attached_->original_nrpn_parameter_lookup={};
        attached_=nullptr;
    }
private:
    MidiEngine* attached_=nullptr;
    std::array<std::uint8_t,kDeviceBytes> device_{};
    std::array<std::uint8_t,kParameterBytes> parameters_{};
    std::array<std::uint16_t,kLookupWords> lookup_{};
};
} // namespace awe64::original_vxd::research
