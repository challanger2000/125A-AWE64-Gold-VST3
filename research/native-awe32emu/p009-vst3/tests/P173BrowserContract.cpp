// P173 independent C++20 browser-origin contract. Do not link Steinberg
// module_win32.cpp in C++20: that pinned SDK expects std::string from u8string.
// The unchanged audio host test remains pinned to its proven C++17 build.
#include "p165/PresetBrowserModel.h"
#include <filesystem>
#include <cstdio>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
void check(bool ok,const char* issue) {
  if(!ok){std::fprintf(stderr,"P173_BROWSER_FAIL %s\n",issue);
    throw std::runtime_error(issue);}
}
// P173: use the original chip's actual drum bank 128, but never claim an
// SF2 replacement kit is the original ROM kit. The test deliberately loads
// a DRUM-ONLY SF2: pre-P173 GUI rejected it as "no melodic presets".
void verifyP173DrumBrowserSource(const fs::path& resources){
  AWE125::PresetCatalog catalog,drum;
  catalog.SetOriginalCreativeMidiNumbers();
  const auto* rom=catalog.Find(128u,0u);
  check(AWE125::PresetBrowserModel::PreferredImportedBank(catalog)==-1,
        "P173 ROM-only catalog falsely reported an imported SoundFont");
  check(rom && !rom->fromUserSf2 &&
        AWE125::PresetBrowserModel::BankLabel(128u,catalog)=="ROM Bank B - Drums",
        "P173 initial original GM drum-kit provenance missing");
  const auto path=resources/"drums.sf2";
  const auto utf8=path.u8string();
  std::string error;
  check(drum.LoadSoundFont(std::string(utf8.begin(),utf8.end()),error),
        "P173 synthetic drum-only SF2 invalid");
  check(catalog.AppendCreativeMelodicUserOneFromSource(drum)==1u,
        "P173 drum-only SoundFont was incorrectly rejected");
  check(AWE125::PresetBrowserModel::PreferredImportedBank(catalog)==128,
        "P173 drum-only SF2 was not chosen for the native CH10 browser lane");
  const auto* userKit=catalog.Find(128u,0u);
  check(userKit && userKit->drums && userKit->fromUserSf2 &&
        userKit->name=="PublicTwoLayers" &&
        AWE125::PresetBrowserModel::BankLabel(128u,catalog)=="SF2 Bank B - Drums",
        "P173 loaded SF2 kit was falsely labelled ROM in browser");
  check(!catalog.Find(129u,0u) && catalog.Entries().size()==129u,
        "P173 native browser fabricated an additional drum bank/presets");
  const auto label=AWE125::PresetBrowserModel::CurrentLabel(
      0,catalog,catalog,128u,0u);
  check(label.find("PublicTwoLayers")!=std::string::npos,
        "P173 actual selected SF2 drum kit not shown");
  const auto missing=AWE125::PresetBrowserModel::CurrentLabel(
      0,catalog,catalog,1u,127u);
  check(missing.find("GM fallback candidate: Gunshot")!=std::string::npos &&
        missing.find("Nicht") == std::string::npos,
        "P173 sparse user-bank fallback still misleading/non-English");
  std::puts("P173_DRUM_ONLY_SF2_ORIGIN_BROWSER_NO_PHANTOM_BANK_AND_ENGLISH_PASS");
}



// P180 independent C++20 browser/model regression: a sparse 77-preset
// SoundFont with absent MIDI program 63 MUST NOT be padded to 78 or 128,
// and Prev/Next MUST skip the hole while preserving all ROM bank 0 names.
void verifyP180Sparse77RealBrowserContract(const fs::path& resources){
  const auto input=resources/"sparse77.sf2";
  AWE125::PresetCatalog sparse,combined;
  const auto u8=input.u8string();
  const std::string path(u8.begin(),u8.end());
  std::string err;
  check(sparse.LoadSoundFont(path,err),
        "P180 valid public sparse 77-SF2 phdr could not parse");
  check(sparse.Entries().size()==77u && !sparse.Find(0u,63u) &&
        sparse.Find(0u,62u) && sparse.Find(0u,64u) &&
        sparse.Find(0u,77u),
        "P180 actual 77 SF2 phdr entries were padded or shifted");
  combined.SetOriginalCreativeMidiNumbers();
  check(combined.AppendCreativeMelodicUserOneFromSource(sparse)==77u,
        "P180 Creative USER bank 1 imported an invented entry");
  check(combined.Entries().size()==206u &&
        AWE125::PresetBrowserModel::PreferredImportedBank(combined)==1,
        "P180 ROM/77 owner/drums provenance or count corrupted");
  for(int prog=0;prog<=77;++prog){
    const auto* loaded=combined.Find(1u,static_cast<std::uint8_t>(prog));
    if(prog==63) {
      check(!loaded && combined.Find(0u,63u),
            "P180 missing user program 63 was fabricated (or ROM lost)");
    }else{
      check(loaded && loaded->fromUserSf2 && loaded->bank==1u &&
            loaded->program==prog,
            "P180 user preset numbers differ from the source SoundFont");
    }
  }
  check(AWE125::PresetBrowserModel::Neighbor(
            0,combined,combined,1u,62u,+1)==64 &&
        AWE125::PresetBrowserModel::Neighbor(
            0,combined,combined,1u,64u,-1)==62 &&
        AWE125::PresetBrowserModel::Neighbor(
            0,combined,combined,1u,77u,+1)==0 &&
        AWE125::PresetBrowserModel::Neighbor(
            0,combined,combined,1u,0u,-1)==77,
        "P180 PREV/NEXT failed sparse program 63 gap or wrap");
  const auto missing=AWE125::PresetBrowserModel::CurrentLabel(
      0,combined,combined,1u,63u);
  check(missing.find("No SF2 preset; GM fallback candidate:")!=
            std::string::npos,
        "P180 missing program63 browser wrongly labels a real SF2 preset");
  std::puts("P180_SPARSE_77_PUBLIC_BROWSER_NO_PHANTOM_PREV_NEXT_ROM_PASS");
}


// P183 real eight-program percussion bank (0..7). This tests the actual
// browser catalog and Prev/Next independently of the DSP and proprietary
// user's eight Vintage Dreams drum-kit programs.
void verifyP183EightOriginalChipDrumKitEntries(const fs::path& resources){
  AWE125::PresetCatalog source,catalog;
  std::string error;
  const auto path=(resources/"drumkits8.sf2").u8string();
  check(source.LoadSoundFont(std::string(path.begin(),path.end()),error),
        "P183 original synthetic 8-kit SoundFont phdr invalid");
  check(source.Entries().size()==8u &&
        source.Find(128u,0u) && source.Find(128u,7u) &&
        !source.Find(128u,8u),
        "P183 source kit0..7 absent or fabricated extra kit8");
  catalog.SetOriginalCreativeMidiNumbers();
  check(catalog.AppendCreativeMelodicUserOneFromSource(source)==8u &&
        catalog.Entries().size()==136u &&
        AWE125::PresetBrowserModel::PreferredImportedBank(catalog)==128,
        "P183 actual Creative drums + ROM catalog count incorrect");
  for(unsigned prog=0;prog<8;++prog){
    const auto* kit=catalog.Find(128u,static_cast<std::uint8_t>(prog));
    check(kit && kit->fromUserSf2 && kit->drums,
          "P183 real bank128 kit lost SF2 origin/number");
    const int prev=prog==0?7:static_cast<int>(prog)-1;
    const int next=prog==7?0:static_cast<int>(prog)+1;
    check(AWE125::PresetBrowserModel::Neighbor(
        0,catalog,catalog,128u,static_cast<std::uint8_t>(prog),-1)==prev &&
          AWE125::PresetBrowserModel::Neighbor(
        0,catalog,catalog,128u,static_cast<std::uint8_t>(prog),+1)==next,
        "P183 drum kit Next/Previous skip or wrong wrap");
  }
  std::puts("P183_EIGHT_SF2_DRUM_KITS_BANK128_BROWSER_PROGRAM_PREV_NEXT_PASS");
}

int main(int argc,char** argv){
  if(argc!=2) {
    std::fprintf(stderr,"Usage: P173BrowserContract <synthetic-SF2-directory>\n");
    return 2;
  }
  try {
    verifyP173DrumBrowserSource(fs::u8path(argv[1]));
    verifyP180Sparse77RealBrowserContract(fs::u8path(argv[1]));
    verifyP183EightOriginalChipDrumKitEntries(fs::u8path(argv[1]));
    return 0;
  }catch(const std::exception& e){
    std::fprintf(stderr,"P173_BROWSER_ERROR %s\n",e.what());
    return 1;
  }
}
