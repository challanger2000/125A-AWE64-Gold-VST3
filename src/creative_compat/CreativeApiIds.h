#pragma once
// P002 observational identifiers ONLY from Creative's 1996 AWE API.
// Source: qb40/sound-blaster-awe32@69e09cbf59fd4ff7dce3482406029c88a05ab878,
// WINDOWS/INCLUDE/AWE_API.H (Creative AWE Developer Package v3.01).
// Pure numeric identifiers; NO original DLL/driver implementation is linked.
#include <cstdint>

namespace awe64::creative {
enum class AweMessage : std::uint16_t {
    Open = 101, Close = 102,
    SelectEffects = 202, SelectSynthBank = 203,
    LoadUserBank = 204, ClearUserBank = 205,
    QueryDramSize = 308,
    SendMidi = 401,
    ViennaStart = 2001,
    ViennaEnd = 2002,
    ViennaLoadSample = 2003,
    ViennaFreeSample = 2004,
    ViennaPlaySample = 2005,
    ViennaLoadPreset = 2006,
    ViennaFreePreset = 2007,
    ViennaNoteOn = 2009,
    ViennaNoteOff = 2010,
    ViennaController = 2011,
    ViennaPitchBend = 2012,
    ViennaChannelPressure = 2013,
    ViennaSysEx = 2014,
    ViennaQueryDramSize = 2016
};
static_assert(static_cast<std::uint16_t>(AweMessage::SendMidi)==401);
static_assert(static_cast<std::uint16_t>(AweMessage::ViennaNoteOn)==2009);
static_assert(static_cast<std::uint16_t>(AweMessage::ViennaController)==2011);
} // namespace awe64::creative
