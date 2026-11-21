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


int main(int argc,char** argv){
  if(argc!=2) {
    std::fprintf(stderr,"Usage: P173BrowserContract <synthetic-SF2-directory>\n");
    return 2;
  }
  try {
    verifyP173DrumBrowserSource(fs::u8path(argv[1]));
    return 0;
  }catch(const std::exception& e){
    std::fprintf(stderr,"P173_BROWSER_ERROR %s\n",e.what());
    return 1;
  }
}
