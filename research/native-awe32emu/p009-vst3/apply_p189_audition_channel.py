"""Give the Win95 adapter its own drum audition channel; leave the chip C core untouched."""
from pathlib import Path
import sys

source = Path(sys.argv[1]) / "AWE32Emu" / "src"
header = source / "Synth.h"
implementation = source / "Synth.cpp"
hpp = header.read_text(encoding="utf-8")
cpp = implementation.read_text(encoding="utf-8")


def replace(text, old, new, count=1):
    if text.count(old) != count:
        raise RuntimeError(f"Unexpected pinned Synth source at {old!r}")
    return text.replace(old, new)


hpp = replace(hpp, "    void NoteOn(uint8_t channel,",
              "    static constexpr uint8_t kAuditionDrumChannel = 16;\n"
              "    static constexpr unsigned kChannelCount = 17;\n"
              "    void NoteOn(uint8_t channel,")
hpp = replace(hpp, "std::array<ChannelState, 16>",
              "std::array<ChannelState, kChannelCount>")
hpp = replace(hpp, "    int  BankNumberFor(uint8_t channel) const;",
              "    bool IsMidiChannel(uint8_t channel) const;\n"
              "    static bool IsDrumChannel(uint8_t channel) {\n"
              "        return channel == 9 || channel == kAuditionDrumChannel;\n"
              "    }\n"
              "    int  BankNumberFor(uint8_t channel) const;")
cpp = replace(cpp, "int Synth::BankNumberFor(uint8_t channel) const",
              "bool Synth::IsMidiChannel(uint8_t channel) const\n"
              "{\n"
              "    return channel < kAuditionDrumChannel ||\n"
              "           (channel == kAuditionDrumChannel &&\n"
              "            m_core.DriverVariant() == Awe32::Driver::Win95);\n"
              "}\n\n"
              "int Synth::BankNumberFor(uint8_t channel) const")
cpp = replace(cpp, "if (channel == 9) return kDrumBank;",
              "if (IsDrumChannel(channel)) return kDrumBank;")
cpp = replace(cpp, "\n    if (channel != 9 && attackRate < 0x7D)",
              "\n    if (!IsDrumChannel(channel) && attackRate < 0x7D)")
cpp = replace(cpp, "if (channel >= 16) return;",
              "if (!IsMidiChannel(channel)) return;", count=5)
cpp = replace(cpp, "if (channel >= 16 || note > 127) return;",
              "if (!IsMidiChannel(channel) || note > 127) return;")
cpp = replace(cpp, "if (!((m_channelMask >> channel) & 1)) return;",
              "const auto sourceChannel = channel == kAuditionDrumChannel ? 0 : channel;\n"
              "    if (!((m_channelMask >> sourceChannel) & 1)) return;")
header.write_text(hpp, encoding="utf-8")
implementation.write_text(cpp, encoding="utf-8")
