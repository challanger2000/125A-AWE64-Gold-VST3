// P210: real 86Box EMU8000 hardware-port proof of the Win95 32-voice cap.
// Four exclusively PUBLIC synthetic SF2 worklists: 31, 32, 33, 37 mono zones.
// 37 zones intentionally reuse 24 independent sample IDs, as a stress shape.
// No original owner ROM/SBK/SF2 samples, names or bytes are used by CI.
#include "Synth.h"
#include "Awe32Driver.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
struct Event { int voice; size_t ordinal; };
struct Trace {
    std::vector<Event> stops, starts, mains, triggers;
    unsigned nonzero = 0;
};
void require(bool ok, const std::string& reason) {
    if (!ok) throw std::runtime_error(reason);
}
Trace capture(const char* rom, const char* sf2, int count) {
    Synth synth(44100);
    std::string error;
    require(synth.LoadWaveRom(rom,error), "P210 synthetic ROM load: " + error);
    synth.Core().SetChipRamKb(28672);
    require(synth.Core().UseBox86Chip(rom,error), "P210 86Box chip init: " + error);
    synth.Core().SetDriver(Awe32::Driver::Win95);
    require(synth.LoadBank(sf2,error,false,1), "P210 synthetic SF2 load: " + error);
    auto* ram = synth.Core().ChipRam();
    require(ram != nullptr, "P210 physical DRAM missing");
    const size_t words = std::min(synth.Core().DramSize(),synth.Core().ChipRamWords());
    std::memcpy(ram,synth.Core().DramData(),words*sizeof(std::int16_t));
    synth.Core().PowerOnInit();
    require(synth.PrepareRealtime(256), "P210 37-layer RT preallocation failed");
    synth.ControlChange(0,0,1);
    synth.ProgramChange(0,0);
    const std::string file = "_p210-port-" + std::to_string(count) + ".trace";
    require(synth.Core().OpenTrace(file.c_str()), "P210 original-chip port trace failed");
    synth.NoteOn(0,84,102);
    synth.Core().CloseTrace();
    std::ifstream in(file);
    require(bool(in), "P210 original-chip trace missing");
    Trace t;
    std::string line;
    unsigned selected = 0;
    size_t ordinal = 0;
    while (std::getline(in,line)) {
        if (line.empty() || line[0]=='#' || line.rfind("R ",0)==0) continue;
        unsigned long long frame=0;
        unsigned port=0, value=0;
        require(std::sscanf(line.c_str(),"%llu %x %x",&frame,&port,&value)==3,
                "P210 malformed genuine-chip port record");
        ++ordinal;
        if (port==0xE22) { selected=value; continue; }
        if (port==0xA20 && (selected & 0xE0)==0xA0) {
            const int v=static_cast<int>(selected & 31);
            if (value==0xFFu) t.stops.push_back({v,ordinal});
            else if ((value & 0x80u)==0u) t.triggers.push_back({v,ordinal});
        }
        if (port==0xE20 && (selected & 0xE0)==0x20)
            t.starts.push_back({static_cast<int>(selected & 31),ordinal});
        if (port==0x622 && selected<32 && value!=0)
            t.mains.push_back({static_cast<int>(selected),ordinal});
    }
    in.close(); std::remove(file.c_str());
    std::vector<std::int16_t> pcm(256*2);
    for (int i=0;i<32;++i) {
        synth.RenderBlock(pcm.data(),256);
        for (auto v:pcm) if (v) ++t.nonzero;
    }
    return t;
}
void check(const Trace& t, int wanted) {
    const int voices=std::min(wanted,32);
    // P212 original Win95 guest: exactly one allocator stop per played voice.
    // The first 33/37-note extra zones must NOT fall back to a reserved v0.
    require(t.stops.size()==static_cast<size_t>(voices),
            "P210_EXCESS_DUPLICATE_VOICE_RESERVATION: " +
            std::to_string(wanted) + " layers produced " +
            std::to_string(t.stops.size()) + " stop writes; expected " +
            std::to_string(2*voices));
    require(t.starts.size()==static_cast<size_t>(voices) &&
            t.mains.size()==static_cast<size_t>(voices) &&
            t.triggers.size()==static_cast<size_t>(voices),
            "P210 hardware voice main/trigger count not bounded by 32");
    std::set<int> reserved, mainVoices, triggered;
    for (int i=0;i<voices;++i) reserved.insert(t.stops[i].voice);
    for (auto e:t.mains) mainVoices.insert(e.voice);
    for (auto e:t.triggers) triggered.insert(e.voice);
    require(reserved.size()==static_cast<size_t>(voices) &&
            reserved==mainVoices && reserved==triggered,
            "P210 reserved physical voice alias or lost note trigger");
    require(t.stops[voices-1].ordinal < t.starts.front().ordinal,
            "P210 all layer reservations must precede first IFATN main");
    require(t.nonzero>64,"P210 original EMU8000 synthesized silence");
    std::cout << "P210_ORIGINAL_CHIP_VOICE_BUDGET_PASS requested=" << wanted
              << " physical=" << voices << " dropped=" << wanted-voices
              << " genuine_pcm_nonzero=" << t.nonzero << "\n";
}
} // namespace
int main(int argc,char** argv) {
    if (argc!=6) return 2;
    try {
        for (int i=0;i<4;++i) {
            const int layers[] = {31,32,33,37};
            check(capture(argv[1],argv[i+2],layers[i]),layers[i]);
        }
        std::puts("P210_86BOX_31_32_33_37_LAYER_32_VOICE_BOUNDARY_PASS");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "P210_NO_GO " << e.what() << "\n";
        return 1;
    }
}
