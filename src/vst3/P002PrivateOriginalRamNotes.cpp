#include "P002PrivateOriginalRamNotes.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>

namespace awe64::vst3 {
namespace {
// Source: one complete real Win95/AWE64 Gold CC0=1 original trace;
// private fixture extracted *verbatim* from EMU8K_TRACE, keeping
// 152+8+156+8=324 authentic chip PORT WRITES in their original order.
// Soundfont and 83 real RAM samples are separately SHA256-pinned by the
// other P002 private original RAM capture activation gate.
constexpr std::string_view kOriginalNotePortSha =
    "3e967768c248c89e67802a14235c2e986d0b091e61cf1024931658384faed484";
constexpr std::size_t kOriginalNotePortBytes=5741u;
constexpr std::array<std::size_t,4> kExactSourceCounts{{152u,8u,156u,8u}};
constexpr std::array<std::uint32_t,4> kSourceFirstFrames{{
    3154566u,3172293u,3201361u,3219527u
}};
constexpr std::array<std::string_view,4> kSourceSections{{
    "# C4_ON","# C4_OFF","# D4_ON","# D4_OFF"
}};

bool hasExactNoteSourceSha(
    const std::span<const std::uint8_t> data) noexcept {
    if(data.size()!=kOriginalNotePortBytes)return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE state=nullptr;
    std::array<UCHAR,32> hash{};
    bool accepted=false;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,
                                   nullptr,0)==0) {
        if(BCryptCreateHash(algorithm,&state,nullptr,0,nullptr,0,0)==0) {
            if(BCryptHashData(state,
                    const_cast<PUCHAR>(reinterpret_cast<const UCHAR*>(
                        data.data())),static_cast<ULONG>(data.size()),0)==0 &&
               BCryptFinishHash(state,hash.data(),
                                static_cast<ULONG>(hash.size()),0)==0) {
                constexpr char digits[]="0123456789abcdef";
                accepted=true;
                for(std::size_t i=0;i<hash.size();++i) {
                    if(kOriginalNotePortSha[2*i]!=digits[hash[i]>>4] ||
                       kOriginalNotePortSha[2*i+1]!=digits[hash[i]&15])
                        accepted=false;
                }
            }
            BCryptDestroyHash(state);
        }
        BCryptCloseAlgorithmProvider(algorithm,0);
    }
    return accepted;
}
bool number(const std::string& text,
            const int base,std::uint32_t& result) noexcept {
    if(text.empty())return false;
    std::uint32_t parsed=0;
    const auto [end,err]=std::from_chars(
        text.data(),text.data()+text.size(),parsed,base);
    if(err!=std::errc{} || end!=text.data()+text.size())return false;
    result=parsed;
    return true;
}
} // namespace

void P002PrivateOriginalRamNotes::clear() noexcept {
    notes_={};
    ready_=false;
}

bool P002PrivateOriginalRamNotes::load(
    const std::filesystem::path& path,std::string& error) {
    clear();
    error.clear();
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file ||
       file.tellg()!=static_cast<std::streamoff>(kOriginalNotePortBytes)) {
        error="Real original Creative C4/D4 note fixture length invalid";
        return false;
    }
    file.seekg(0,std::ios::beg);
    std::array<std::uint8_t,kOriginalNotePortBytes> bytes{};
    if(!file.read(reinterpret_cast<char*>(bytes.data()),bytes.size()) ||
       !hasExactNoteSourceSha(bytes)) {
        error="Real original Creative C4/D4 note fixture SHA256 invalid";
        return false;
    }
    const std::string text(reinterpret_cast<const char*>(bytes.data()),
                           bytes.size());
    std::istringstream lines(text);
    std::string line;
    std::size_t section=0u,section_count=0u;
    bool anySection=false;
    std::array<bool,4> seenSections{};
    while(std::getline(lines,line)) {
        if(line.empty())continue;
        if(line[0]=='#') {
            for(std::size_t i=0;i<kSourceSections.size();++i) {
                if(line==kSourceSections[i]) {
                    if(seenSections[i] ||
                       (anySection && section_count!=kExactSourceCounts[section]) ||
                       (anySection && i!=section+1u) ||
                       (!anySection && i!=0u)) {
                        error="Original Creative source note groups/order invalid";
                        clear();return false;
                    }
                    section=i;
                    section_count=0u;
                    seenSections[i]=true;
                    anySection=true;
                    break;
                }
            }
            continue; // other provenance comments are SHA256-protected
        }
        if(!anySection || section_count>=kExactSourceCounts[section]) {
            error="Original Creative source note event count invalid";
            clear();return false;
        }
        std::istringstream cells(line);
        std::string f,p,v,extra;
        if(!(cells>>f>>p>>v) || cells>>extra) {
            error="Original Creative note port line malformed";
            clear();return false;
        }
        std::uint32_t frame=0u,port=0u,value=0u;
        if(!number(f,10,frame) || !number(p,16,port) ||
           !number(v,16,value) ||
           frame<kSourceFirstFrames[section] ||
           frame-kSourceFirstFrames[section]>(section%2u==0u?2u:0u) ||
           port>0xffffu || value>0xffffu ||
           (port!=0xe22u && port!=0xe20u &&
            port!=0xa20u && port!=0xa22u &&
            port!=0x620u && port!=0x622u)) {
            error="Original Creative source note port/frame invalid";
            clear();return false;
        }
        const Event event{static_cast<std::uint16_t>(
                              frame-kSourceFirstFrames[section]),
                          static_cast<std::uint16_t>(port),
                          static_cast<std::uint16_t>(value)};
        auto& group=notes_[section/2u];
        auto& clip=(section%2u==0u?group.on:group.off);
        if(clip.count>0u &&
           event.relative_chip_frame<
               clip.events[clip.count-1u].relative_chip_frame) {
            error="Original source chip clock nonmonotonic";clear();
            return false;
        }
        if(clip.count>=kMaxSourceEvents) {
            error="Original Creative note port event bound exceeded";
            clear();return false;
        }
        clip.events[clip.count++]=event;
        ++section_count;
    }
    if(!seenSections[0] || !seenSections[1] ||
       !seenSections[2] || !seenSections[3] ||
       section_count!=kExactSourceCounts[3] ||
       notes_[0].on.count!=kExactSourceCounts[0] ||
       notes_[0].off.count!=kExactSourceCounts[1] ||
       notes_[1].on.count!=kExactSourceCounts[2] ||
       notes_[1].off.count!=kExactSourceCounts[3]) {
        error="Original source NoteOn/NoteOff fixture incomplete";
        clear();return false;
    }
    ready_=true;
    return true;
}

bool P002PrivateOriginalRamNotes::verifiedNativeClockPlan(
    const bool note_off,const int channel,const int pitch,
    const int velocity,
    const std::span<const awe64::ChipPortWrite> generated,
    const std::span<awe64::creative::SourceClockedPort> output,
    std::size_t& output_count) const noexcept {
    output_count=0u;
    // Only the owner-verified RAM-SF2 C4/D4 session is eligible.
    if(!ready_ || channel!=0 || (pitch!=60 && pitch!=62) ||
       (!note_off && velocity!=100))
        return false;
    const auto& group=notes_[pitch==60?0u:1u];
    const auto& source=note_off?group.off:group.on;
    if(source.count==0u || source.count>kMaxSourceEvents)return false;
    std::array<awe64::creative::SourceClockedPort,kMaxSourceEvents>
        originalFrames{};
    for(std::size_t i=0u;i<source.count;++i) {
        const auto& e=source.events[i];
        originalFrames[i]={e.relative_chip_frame,e.port,e.value};
    }
    // The cross-platform helper is regression-tested against arbitrary
    // synthetic source-note events. Only this private loader supplies
    // SHA-pinned real Win95 Creative events in the exported VST3.
    return awe64::creative::checkedNativePortsAtSourceFrames(
        std::span<const awe64::creative::SourceClockedPort>(
            originalFrames.data(),source.count),
        generated,output,output_count);
}

void P002PrivateOriginalRamNotes::executeFrame(
    awe64::Chip& chip,Group& g) noexcept {
    while(g.next_on<g.on.count &&
          g.on.events[g.next_on].relative_chip_frame==g.elapsed) {
        const auto& p=g.on.events[g.next_on];
        chip.writePort(p.port,p.value);
        ++g.next_on;
    }
}
bool P002PrivateOriginalRamNotes::noteOn(
    awe64::Chip& chip,const int channel,const int pitch,
    const int velocity) noexcept {
    if(!ready_ || !chip.ready() || channel!=0 || velocity!=100 ||
       (pitch!=60 && pitch!=62))
        return false;
    auto& g=notes_[pitch==60?0u:1u];
    if(g.active || g.started)return false; // no unverified voice stealing
    g.active=true;
    g.started=true;
    g.elapsed=0u;
    g.next_on=0u;
    executeFrame(chip,g);
    return true;
}
bool P002PrivateOriginalRamNotes::noteOff(
    awe64::Chip& chip,const int channel,const int pitch) noexcept {
    if(!ready_ || !chip.ready() || channel!=0 ||
       (pitch!=60 && pitch!=62))return false;
    auto& g=notes_[pitch==60?0u:1u];
    if(!g.active || g.next_on!=g.on.count)return false;
    for(std::size_t i=0u;i<g.off.count;++i) {
        const auto& p=g.off.events[i];
        chip.writePort(p.port,p.value);
    }
    g.active=false;
    return true;
}
void P002PrivateOriginalRamNotes::render(
    awe64::Chip& chip,std::int32_t* audio,
    const std::size_t frames) noexcept {
    std::size_t done=0u;
    while(done<frames) {
        for(auto& g:notes_)
            if(g.active)executeFrame(chip,g);
        std::size_t advance=frames-done;
        for(const auto& g:notes_) {
            if(!g.active || g.next_on>=g.on.count)continue;
            const auto next=g.on.events[g.next_on].relative_chip_frame;
            if(next>=g.elapsed)
                advance=std::min<std::size_t>(
                    advance,next-g.elapsed);
        }
        if(advance==0u) {
            // Only possible if an event with timestamp equal to the
            // current source sample was not consumed. Fail closed
            // rather than spin forever or fabricate event timing.
            bool progress=false;
            for(auto& g:notes_) {
                if(g.active && g.next_on<g.on.count &&
                   g.on.events[g.next_on].relative_chip_frame==g.elapsed) {
                    executeFrame(chip,g);
                    progress=true;
                }
            }
            if(progress)continue;
            // In an impossible corrupted state render the remaining
            // original chip without modifying unverified voice ports.
            advance=frames-done;
        }
        chip.render(audio+2u*done,advance);
        done+=advance;
        for(auto& g:notes_)
            if(g.active)g.elapsed+=static_cast<std::uint32_t>(advance);
    }
}
} // namespace awe64::vst3
