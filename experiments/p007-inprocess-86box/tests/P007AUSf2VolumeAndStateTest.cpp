// P007AU PUBLIC synthetic-only integration of:
 // a separate FAT16 IDE guest SF2 volume, 86Box config and VST3 recall.
 // DO NOT claim original Win95/Creative PCM, no ROM or private image here.
#include "../vst3/P007Sf2FatVolume.hpp"
#include "../vst3/P007Sf2GuestProfile.hpp"
#include "../vst3/P007StateCodec.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
using namespace p007;
namespace fs=std::filesystem;
void check(bool ok,const char* reason) {
    if(!ok){std::fprintf(stderr,"P007AU_FAIL %s\n",reason);std::exit(1);}
}
std::vector<std::uint8_t> read(const fs::path& p,std::uint64_t offset,unsigned n){
    std::ifstream file(p,std::ios::binary);
    file.seekg(static_cast<std::streamoff>(offset));
    std::vector<std::uint8_t> data(n);
    check(bool(file.read(reinterpret_cast<char*>(data.data()),n)),
          "read guest disk failed");
    return data;
}
int main(){
    const auto base=fs::temp_directory_path()/"p007au-fat16-synthetic-qa";
    std::error_code ec;fs::remove_all(base,ec);fs::create_directories(base);
    const auto font=base/"example.sf2";
    std::vector<std::uint8_t> data(8192u);
    data[0]='R';data[1]='I';data[2]='F';data[3]='F';
    sf2_volume::le32(data.data()+4u,8192u-8u);
    data[8]='s';data[9]='f';data[10]='b';data[11]='k';
    for(unsigned i=12u;i<data.size();++i)
        data[i]=static_cast<std::uint8_t>((i*37u)&255u);
    {std::ofstream out(font,std::ios::binary);
     out.write(reinterpret_cast<char*>(data.data()),data.size());}
    const auto image=base/"vm"/"p007sf2.img";
    std::string err;
    check(sf2_volume::build(image,font,err),"valid synthetic RIFF copy rejected");
    check(fs::file_size(image)==sf2_volume::imageBytes,
          "FAT16 image geometry wrong");
    const auto mbr=read(image,0,512u);
    check(mbr[510]==0x55u&&mbr[511]==0xaau&&mbr[450]==0x06u,
          "FAT16 MBR invalid");
    check(sf2_volume::read32(mbr.data()+454u)==63u &&
          sf2_volume::read32(mbr.data()+458u)==
              sf2_volume::partitionSectors,"secondary IDE partition wrong");
    const auto bpb=read(image,63u*512u,512u);
    check(bpb[510]==0x55u&&bpb[511]==0xaau &&
          bpb[13]==8u&&bpb[16]==2u,"guest FAT16 BPB invalid");
    const auto fat1=read(image,64u*512u,64u*512u);
    const auto fat2=read(image,128u*512u,64u*512u);
    check(fat1==fat2,"guest FAT mirror mismatch");
    check(fat1[4]==3u&&fat1[5]==0u&&fat1[6]==0xffu&&fat1[7]==0xffu,
          "expected two-cluster SoundFont FAT chain missing");
    const auto root=read(image,(63u+1u+128u)*512u,32u);
    check(std::memcmp(root.data(),"P007FONTSF2",11u)==0 &&
          root[11]==0x20u &&
          sf2_volume::read32(root.data()+28u)==data.size(),
          "guest SoundFont root directory incorrect");
    const auto guest=read(image,std::uint64_t(sf2_volume::dataStart)*512u,
                          static_cast<unsigned>(data.size()));
    check(guest==data,"generated guest SoundFont differs byte for byte");

    // Corrupt source must be rejected without touching an existing disk.
    const auto oldRoot=root;
    data[0]='X';
    {std::ofstream out(font,std::ios::binary|std::ios::trunc);
     out.write(reinterpret_cast<char*>(data.data()),data.size());}
    check(!sf2_volume::build(image,font,err),
          "broken original RIFF header accidentally accepted");
    check(read(image,(63u+1u+128u)*512u,32u)==oldRoot,
          "failed SoundFont staging damaged previous guest disk");
    data[0]='R';
    {std::ofstream out(font,std::ios::binary|std::ios::trunc);
     out.write(reinterpret_cast<char*>(data.data()),data.size());}
    check(sf2_volume::build(image,font,err),"valid SF2 replacement refused");

    const auto source=base/"P007_AWE64.original.cfg";
    const auto destination=base/"P007_AWE64.cfg";
    const std::string cfg="[General]\r\nfoo = true\r\n"
        "[Hard disks]\r\nhdd_01_fn = win95.img\r\n"
        "hdd_01_ide_channel = 0:0\r\n"
        "[Ports (COM & LPT)]\r\nserial2_enabled = 1\r\n";
    {std::ofstream out(source,std::ios::binary);
     out.write(cfg.data(),cfg.size());}
    check(sf2_profile::configure(source,destination,true,err),
          "guest data disk profile rejected");
    std::ifstream in(destination,std::ios::binary);
    const std::string enabled((std::istreambuf_iterator<char>(in)),{});
    in.close();
    check(enabled.find("hdd_01_fn = win95.img")!=std::string::npos &&
          enabled.find("hdd_02_fn = p007sf2.img")!=std::string::npos &&
          enabled.find("hdd_02_ide_channel = 0:1")!=std::string::npos,
          "config changed original OS disk or omitted new slave");
    check(sf2_profile::configure(source,destination,false,err),
          "ROM-only profile rejected");
    std::ifstream off(destination,std::ios::binary);
    const std::string disabled((std::istreambuf_iterator<char>(off)),{});
    check(disabled==cfg,"ROM-only profile lost byte-for-byte original config");

    state::Values wanted{};
    wanted.bank=1u;wanted.program=40u;wanted.master=7500u;
    wanted.modulation[8]=3200u;
    auto imageState=state::encodeWithSf2(wanted,"C:/example/last bank.sf2");
    check(!imageState.empty(),"SF2 project state not encoded");
    state::Values got{};std::string path;
    check(state::decodeWithSf2(imageState,got,path),
          "SF2 state v3 failed legitimate roundtrip");
    check(path=="C:/example/last bank.sf2" && got.bank==1u &&
          got.program==40u && got.master==7500u &&
          got.modulation[8]==3200u,"SF2 recall values not restored");
    imageState[48]^=1u;
    check(!state::decodeWithSf2(imageState,got,path),
          "SF2 path corruption not detected");
    const auto legacy=state::encode(wanted);
    state::Values old{};
    check(state::decode(legacy,old)&&old.bank==1u,
          "SF2 feature broke historical 44-byte VST3 state");

    fs::remove_all(base,ec);
    std::puts("P007AU_SYNTHETIC_FAT16_SECOND_IDE_SF2_AND_V3_STATE_QA_PASS");
    std::puts("P007AU_NO_ORIGINAL_WIN95_SF2_PCM_CLAIM");
}
