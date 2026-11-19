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
    std::puts("P007BB_ORIGINAL_GM_ROM_128_NAMES_WITH_SF2_SPARSE_BANK1_UNIT_PASS");
}
