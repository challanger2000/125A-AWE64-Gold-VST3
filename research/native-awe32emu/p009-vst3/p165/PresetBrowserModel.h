#pragma once
// Testable, non-realtime preset browsing logic. Actual source metadata only.
#include "PresetCatalog.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace AWE125 {
class PresetBrowserModel {
public:
    struct Row {
        uint16_t bank=0;
        uint8_t program=0;
        std::string name;
        bool drums=false;
    };
    static const char* HardwareDescription(int engine) noexcept {
        switch(engine) {
        case 0:return "Sample synthesizer with wave ROM and SoundFonts (SF2).";
        case 1:return "Original Yamaha FM synthesizer for classic Sound Blaster sounds.";
        case 2:return "Creative's own OPL3-compatible FM synthesizer.";
        default:return "Unknown hardware.";
        }
    }
    static const char* HardwareName(int engine) noexcept {
        switch(engine) {
        case 0:return "EMU8000 AWE32/64";
        case 1:return "Yamaha OPL3";
        case 2:return "Creative CQM";
        default:return "Unknown";
        }
    }
    static std::string BankLabel(uint16_t bank,const PresetCatalog& catalog) {
        if(bank==0u)return "ROM Bank A - Instruments";
        if(bank==1u)return "SF2 Bank A - Instruments";
        if(bank==128u) {
            bool hasRom=false,hasSf2=false;
            for(const auto& e:catalog.Entries())if(e.bank==128u){
                if(e.fromUserSf2)hasSf2=true;
                else hasRom=true;
            }
            if(hasRom&&hasSf2)return "ROM / SF2 Bank B - Drums";
            if(hasSf2)return "SF2 Bank B - Drums";
            return "ROM Bank B - Drums";
        }
        return "Bank "+std::to_string(bank);
    }
    static const PresetCatalog& Active(int engine,const PresetCatalog& sf,
                                       const PresetCatalog& fm) noexcept {
        return engine==0?sf:fm;
    }
    static std::vector<uint16_t> Banks(int engine,const PresetCatalog& sf,
                                       const PresetCatalog& fm) {
        std::vector<uint16_t> out;
        for(const auto& p:Active(engine,sf,fm).Entries())out.push_back(p.bank);
        std::sort(out.begin(),out.end());
        out.erase(std::unique(out.begin(),out.end()),out.end());
        return out;
    }
    static std::vector<Row> Filter(int engine,const PresetCatalog& sf,
                                    const PresetCatalog& fm,int bankFilter,
                                    std::string_view search) {
        std::vector<Row> out;
        const auto needle=Lower(search);
        for(const auto& p:Active(engine,sf,fm).Entries()) {
            if(bankFilter>=0&&p.bank!=bankFilter)continue;
            if(!needle.empty()&&Lower(p.name).find(needle)==std::string::npos)continue;
            out.push_back({p.bank,p.program,p.name,p.drums});
        }
        return out;
    }
    static std::string CurrentLabel(int engine,const PresetCatalog& sf,
                                    const PresetCatalog& fm,uint16_t bank,
                                    uint8_t program) {
        const auto& catalog=Active(engine,sf,fm);
        const auto* p=catalog.Find(engine==0?bank:0,program);
        char prefix[36]{};
        if(engine==0)std::snprintf(prefix,sizeof(prefix),"%u:%03u  ",bank,program);
        else std::snprintf(prefix,sizeof(prefix),"%03u  ",program);
        if(p)return std::string(prefix)+p->name;
        // The original Creative Win95 voice selector tries GM bank 0
        // for a missing melodic SF2 program. Its final patch still
        // depends on key/velocity, so this is a CANDIDATE, not an
        // unverified assertion about actual ROM PCM.
        if(engine==0&&bank==1u){
            if(const auto* gm=catalog.Find(0u,program))
                return std::string(prefix)+"No SF2 preset; GM fallback candidate: "+gm->name;
            return std::string(prefix)+"No SF2 preset; fallback depends on note.";
        }
        if(engine==0&&bank==128u){
            if(const auto* kit=catalog.Find(128u,0u))
                return std::string(prefix)+"Drum kit fallback candidate: "+kit->name;
        }
        return std::string(prefix)+"Preset not available.";
    }
    // P117 direct bank/program selector: only actual catalog entries, no
    // assumed GM-contiguous presets in the user's sparse SF2. Pure logic
    // is unit tested independently of VSTGUI or the host.
    static std::vector<uint8_t> Programs(const std::vector<PresetEntry>& entries,
                                         uint16_t bank) {
        std::vector<uint8_t> out;
        for(const auto& e:entries)if(e.bank==bank)out.push_back(e.program);
        std::sort(out.begin(),out.end());
        out.erase(std::unique(out.begin(),out.end()),out.end());
        return out;
    }
    static uint8_t ChooseExistingProgram(const std::vector<PresetEntry>& entries,
                                         uint16_t bank,uint8_t current) {
        const auto programs=Programs(entries,bank);
        if(programs.empty())return current; // caller must not emit a host edit
        return std::binary_search(programs.begin(),programs.end(),current)
            ? current : programs.front();
    }
    // PREV/NEXT navigates actual entries rather than nonexistent SF2 slots.
    // Search/filter does not alter normal instrument navigation.
    static int Neighbor(int engine,const PresetCatalog& sf,
                        const PresetCatalog& fm,uint16_t bank,
                        uint8_t program,int step) {
        const auto& entries=Active(engine,sf,fm).Entries();
        std::vector<uint8_t> programs;
        for(const auto& p:entries)
            if(p.bank==(engine==0?bank:0))programs.push_back(p.program);
        if(programs.empty())return -1;
        std::sort(programs.begin(),programs.end());
        programs.erase(std::unique(programs.begin(),programs.end()),programs.end());
        const auto it=std::lower_bound(programs.begin(),programs.end(),program);
        if(step>0) {
            auto next=it;
            if(next!=programs.end()&&*next==program)++next;
            return next==programs.end()?programs.front():*next;
        }
        if(it==programs.begin())return programs.back();
        return *(it-1);
    }
private:
    static std::string Lower(std::string_view v) {
        std::string out;
        out.reserve(v.size());
        for(unsigned char c:v)out.push_back(static_cast<char>(
            c<128?std::tolower(c):c));
        return out;
    }
};
} // namespace AWE125
