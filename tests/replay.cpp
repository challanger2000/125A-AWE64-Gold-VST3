#include "Chip.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {
struct Event { uint64_t frame; uint16_t port,value; bool read; };
void put16(std::ostream& o, uint16_t v) { o.put(char(v)); o.put(char(v>>8)); }
void put32(std::ostream& o, uint32_t v) { put16(o, uint16_t(v)); put16(o, uint16_t(v>>16)); }
bool loadTrace(const std::filesystem::path& path, std::vector<Event>& events, std::string& err) {
    std::ifstream f(path);
    if (!f) { err="missing independent fork EMU8K_TRACE"; return false; }
    std::string line;
    uint64_t previous=0;
    size_t lineNo=0;
    while (std::getline(f,line)) {
        ++lineNo;
        if (line.empty() || line[0]=='#') continue;
        std::istringstream in(line);
        Event e{}; std::string first, port, value, extra;
        if (!(in>>first)) continue;
        if (first=="R") { e.read=true; if (!(in>>first)) {err="bad read frame";return false;} }
        if (!(in>>port>>value) || (in>>extra)) { err="bad trace columns at line "+std::to_string(lineNo);return false; }
        try {
            size_t used=0; e.frame=std::stoull(first,&used,10);
            if (used!=first.size()) throw std::invalid_argument("frame");
            unsigned long p=std::stoul(port,&used,16);
            if (used!=port.size() || p>65535) throw std::invalid_argument("port");
            e.port=static_cast<uint16_t>(p);
            unsigned long v=std::stoul(value,&used,16);
            if (used!=value.size() || v>65535) throw std::invalid_argument("value");
            e.value=static_cast<uint16_t>(v);
        } catch (...) {err="invalid trace value at line "+std::to_string(lineNo);return false;}
        if (e.frame<previous || e.frame>44100ull*3600ull) {
            err="non-monotonic or overlong trace at line "+std::to_string(lineNo); return false;
        }
        previous=e.frame; events.push_back(e);
    }
    if (events.empty()) {err="empty trace (not accepted as PCM oracle)";return false;}
    return true;
}
int16_t clampPcm16(int32_t v) {
    return static_cast<int16_t>(std::clamp<int32_t>(v,-32768,32767));
}
void waveHeader(std::ostream& out, uint32_t nFrames) {
    out.write("RIFF",4); put32(out,36u+nFrames*4u);
    out.write("WAVEfmt ",8);put32(out,16);
    put16(out,1);put16(out,2);put32(out,44100);put32(out,44100*4);
    put16(out,4);put16(out,16);out.write("data",4);put32(out,nFrames*4u);
}
bool verifyWavHeader(std::ifstream& ref, uint32_t& frames, std::string& err) {
    std::array<unsigned char,44> h{};
    if (!ref.read(reinterpret_cast<char*>(h.data()),h.size())) {err="reference WAV truncated";return false;}
    auto u16=[&](int o){return unsigned(h[o])|(unsigned(h[o+1])<<8);};
    auto u32=[&](int o){return unsigned(h[o])|(unsigned(h[o+1])<<8)|(unsigned(h[o+2])<<16)|(unsigned(h[o+3])<<24);};
    if (std::string(reinterpret_cast<char*>(h.data()),4)!="RIFF" ||
        std::string(reinterpret_cast<char*>(h.data()+8),4)!="WAVE" ||
        std::string(reinterpret_cast<char*>(h.data()+12),4)!="fmt " ||
        std::string(reinterpret_cast<char*>(h.data()+36),4)!="data" ||
        u32(16)!=16 || u16(20)!=1 || u16(22)!=2 || u32(24)!=44100 ||
        u16(34)!=16 || u32(40)%4!=0) {
        err="fork WAV must be 44100 Hz stereo PCM16 (86Box AWE32_WAV format)";return false;
    }
    frames=u32(40)/4;
    return true;
}
}
int main(int argc, char** argv) {
    if (argc!=9 || std::string(argv[1])!="--rom" ||
        std::string(argv[3])!="--trace" || std::string(argv[5])!="--fork-wav" ||
        std::string(argv[7])!="--out") {
        std::cerr << "usage: awe64_replay --rom private/awe32.raw --trace original.trace "
                  << "--fork-wav original.wav --out native.wav\n";
        return 2;
    }
    std::string err;
    std::vector<Event> trace;
    if (!loadTrace(argv[4],trace,err)) {std::cerr<<"FAIL "<<err<<"\n";return 3;}
    // Comparison always needs a separately captured 86Box AWE32_WAV.
    std::ifstream reference(argv[6],std::ios::binary);
    uint32_t referenceFrames=0;
    if (!verifyWavHeader(reference,referenceFrames,err)) {
        std::cerr<<"NOT VERIFIED: "<<err<<"\n";return 4;
    }
    if (trace.back().frame>=referenceFrames ||
        referenceFrames>44100u*3600u) {
        std::cerr<<"NOT VERIFIED: trace events must fit the recorded WAV duration. "
                  <<"Confirm common chip clock origin for independent captures.\n";
        return 5;
    }
    awe64::Chip chip;
    if (!chip.open(argv[2],err)) {std::cerr<<"FAIL ROM: "<<err<<"\n";return 6;}
    std::ofstream native(argv[8],std::ios::binary|std::ios::trunc);
    if (!native) {std::cerr<<"FAIL opening native output\n";return 7;}
    waveHeader(native,referenceFrames);
    std::array<int32_t,980u*2u> samples{};
    uint64_t frame=0, compared=0;
    size_t eventIndex=0;
    while (frame<referenceFrames) {
        while (eventIndex<trace.size() && trace[eventIndex].frame==frame) {
            const auto& e=trace[eventIndex++];
            if (e.read) {
                const auto result=chip.readPort(e.port);
                if (result!=e.value) {
                    std::cerr<<"FAIL register read at frame "<<frame<<" port 0x"
                             <<std::hex<<e.port<<" expected 0x"<<e.value
                             <<" got 0x"<<result<<std::dec<<"\n";return 8;
                }
            } else chip.writePort(e.port,e.value);
        }
        const uint64_t next=(eventIndex<trace.size()?trace[eventIndex].frame:referenceFrames);
        const auto count=static_cast<size_t>(std::min<uint64_t>({next-frame,980u,referenceFrames-frame}));
        if (!count) {std::cerr<<"FAIL nonprogress trace\n";return 9;}
        chip.render(samples.data(),count);
        for (size_t i=0;i<count*2u;i++) {
            const int16_t actual=clampPcm16(samples[i]);
            const int a=reference.get(), b=reference.get();
            if (a==EOF || b==EOF) {std::cerr<<"FAIL truncated reference PCM\n";return 10;}
            const auto expected=static_cast<int16_t>(uint16_t(a)|(uint16_t(b)<<8));
            put16(native,static_cast<uint16_t>(actual));
            if (actual!=expected) {
                std::cerr<<"PCM PARITY FAIL: first differing frame "<<(frame+i/2u)
                         <<" channel "<<(i%2u)<<" fork "<<expected
                         <<" native "<<actual<<" events "<<eventIndex<<"\n";return 11;
            }
            ++compared;
        }
        frame+=count;
    }
    native.flush();
    if (!native) {std::cerr<<"FAIL writing output\n";return 12;}
    if (reference.get()!=EOF) {std::cerr<<"NOT VERIFIED: WAV contains unframed trailing data\n";return 13;}
    std::cout<<"P001 fork-vs-native PCM16 IDENTICAL: "<<frame<<" stereo frames, "
             <<compared<<" individual samples, "<<trace.size()<<" register events\n";
    return 0;
}
