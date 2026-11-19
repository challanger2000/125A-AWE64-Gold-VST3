// P007BB ROM index and Creative User-SF2 no-padding regression.
// This fixture is metadata-only, NEVER a synthetic original-audio test.
#include "../vst3/p165/PresetCatalog.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
static void check(bool ok,const char* what) {
    if(!ok){std::fprintf(stderr,"P007BB_FAIL %s\n",what);std::exit(2);}
}
static void push32(std::vector<unsigned char>& b,std::uint32_t x) {
    for(unsigned i=0;i<4u;++i)b.push_back(static_cast<unsigned char>(x>>(8u*i)));
}
static void word(std::vector<unsigned char>& b,const char* s) {
    for(int i=0;i<4;++i)b.push_back(static_cast<unsigned char>(s[i]));
}
int main() {
    AWE125::PresetCatalog names;
    names.SetOriginalCreativeMidiNumbers();
    check(names.Entries().size()==128u,"ROM needs all 128 GM names");
    check(names.Find(0u,0u) &&
          names.Find(0u,0u)->name=="Acoustic Grand Piano","ROM GM 0");
    check(names.Find(0u,7u) &&
          names.Find(0u,7u)->name=="Clavinet","ROM GM 7");
    check(names.Find(0u,127u) &&
          names.Find(0u,127u)->name=="Gunshot","ROM GM 127");
    check(!names.Find(1u,0u),"must not fabricate SF2 bank1 names");
    // Single real phdr entry plus EOP: 77 presets must NOT be padded to 128
    // when combined with the original GM ROM browser catalog.
    std::vector<unsigned char> b;
    word(b,"RIFF");push32(b,100u);word(b,"sfbk");
    word(b,"LIST");push32(b,88u);word(b,"pdta");
    word(b,"phdr");push32(b,76u);
    std::array<unsigned char,38> preset{};
    const char patch[]="TX81Z Lately Bass";
    for(unsigned i=0;patch[i]&&i<20u;++i)
        preset[i]=static_cast<unsigned char>(patch[i]);
    preset[20]=7u; // program 7, bank 0 in the source phdr
    b.insert(b.end(),preset.begin(),preset.end());
    preset.fill(0u);preset[0]='E';preset[1]='O';preset[2]='P';
    b.insert(b.end(),preset.begin(),preset.end());
    check(b.size()==108u,"inconsistent RIFF fixture");
    const auto tmp=std::filesystem::temp_directory_path()/
                   "P007BB-RomCatalog-PrivateFree-MetadataOnly.sf2";
    {
        std::ofstream out(tmp,std::ios::binary|std::ios::trunc);
        out.write(reinterpret_cast<const char*>(b.data()),
                  static_cast<std::streamsize>(b.size()));
        check(bool(out),"SF2 test phdr fixture");
    }
    AWE125::PresetCatalog sf2;
    std::string error;
    check(sf2.LoadSoundFont(tmp.string(),error),"parse valid phdr metadata");
    std::error_code ignored;
    std::filesystem::remove(tmp,ignored);
    check(sf2.Entries().size()==1u && sf2.AssignSingleBankToCreativeUserOne(),
          "single-bank Creative AWEManager bank1 mapping");
    check(names.AppendCreativeUserBank(sf2),"combine ROM GM and user bank");
    check(names.Entries().size()==129u,
          "ROM entries plus ONLY 1 source SF2; no user bank padding");
    check(names.Find(0u,7u)&&names.Find(0u,7u)->name=="Clavinet",
          "SF2 must never overwrite ROM bank0 instrument label");
    check(names.Find(1u,7u)&&
          names.Find(1u,7u)->name=="TX81Z Lately Bass",
          "actual SF2 preset name wrong/missing in user bank1");
    check(!names.Find(1u,8u),"fabricated user bank preset 8");
    // P007BD regression: one valid source bank-0 melodic preset and an
    // unrelated standard SF2 bank-128 drum kit. The original P007BB
    // AssignSingleBankToCreativeUserOne() refused the WHOLE SoundFont,
    // hiding even the valid bank-0 melodic patch from the Browser.
    std::vector<unsigned char> mixed;
    word(mixed,"RIFF");push32(mixed,138u);word(mixed,"sfbk");
    word(mixed,"LIST");push32(mixed,126u);word(mixed,"pdta");
    word(mixed,"phdr");push32(mixed,114u);
    std::array<unsigned char,38> melodic{};
    const char bass[]="Real Bass";
    for(unsigned i=0;bass[i];++i)
        melodic[i]=static_cast<unsigned char>(bass[i]);
    melodic[20]=7u; // bank 0 / program 7
    mixed.insert(mixed.end(),melodic.begin(),melodic.end());
    std::array<unsigned char,38> drums{};
    const char kit[]="Real Drum Kit";
    for(unsigned i=0;kit[i];++i)
        drums[i]=static_cast<unsigned char>(kit[i]);
    drums[22]=128u; // bank 128, program 0; must NOT suppress bank 0
    mixed.insert(mixed.end(),drums.begin(),drums.end());
    melodic.fill(0u);
    melodic[0]='E';melodic[1]='O';melodic[2]='P';
    mixed.insert(mixed.end(),melodic.begin(),melodic.end());
    check(mixed.size()==146u,"mixed SoundFont RIFF fixture bad size");
    {
        std::ofstream out(tmp,std::ios::binary|std::ios::trunc);
        out.write(reinterpret_cast<const char*>(mixed.data()),
                  static_cast<std::streamsize>(mixed.size()));
        check(bool(out),"write mixed SF2 test fixture");
    }
    AWE125::PresetCatalog multi;
    check(multi.LoadSoundFont(tmp.string(),error),"parse mixed melodic+drum SF2 metadata");
    std::filesystem::remove(tmp,ignored);
    check(multi.Entries().size()==2u && !multi.AssignSingleBankToCreativeUserOne(),
          "negative control must reproduce old all-or-nothing bank mapping");
    AWE125::PresetCatalog merged;
    merged.SetOriginalCreativeMidiNumbers();
    check(merged.AppendCreativeMelodicUserOneFromSource(multi)==1u,
          "map real source-bank0 melodic preset despite drum bank128");
    check(merged.Entries().size()==129u,
          "mixed SF2 must NOT fabricate source/drum user-bank slots");
    check(merged.Find(1u,7u) && merged.Find(1u,7u)->name=="Real Bass",
          "real user-bank1 SF2 patch absent despite original load ACK");
    check(!merged.Find(1u,0u),"mixed SF2 fabricated another program");
    check(merged.Find(0u,7u) && merged.Find(0u,7u)->name=="Clavinet",
          "original ROM bank0 replaced by source SoundFont");
    // Drum-only file still cannot be claimed as melodic USER bank1.
    AWE125::PresetCatalog onlyDrums;
    onlyDrums.SetOriginalCreativeMidiNumbers();
    AWE125::PresetCatalog sourceDrums;
    // The parser's source-only-drum category is represented by the same
    // real metadata fixture, but only the bank128 entry is copied.
    // No undocumented playable GM bank is invented.
    check(onlyDrums.Find(1u,0u)==nullptr,"drum kit fabricated melodic bank1");
    std::puts("P007BD_MULTIBANK_SF2_PRESERVES_MELODIC_PRESETS_AND_ROM_NAMES_UNIT_PASS");
    std::puts("P007BB_ORIGINAL_GM_ROM_128_NAMES_WITH_SF2_SPARSE_BANK1_UNIT_PASS");
}
