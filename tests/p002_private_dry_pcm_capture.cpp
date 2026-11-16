// Separate Windows-only, P002 exact WAV writer contract.
// No proprietary Creative, ROM, SF2 or original-user audio is linked.
#include "P002PrivateDryPcmCapture.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <cstring>
#define CHECK(x) do { if(!(x)) { \
 std::cerr<<"P002_NATIVE_WAV_WRITER_FAIL line "<<__LINE__<<" "<<#x<<"\n"; \
 return 1; } }while(false)

static std::uint32_t le32(const std::vector<std::uint8_t>& b,
                          std::size_t i) noexcept {
    return std::uint32_t(b[i])|
        (std::uint32_t(b[i+1])<<8u)|
        (std::uint32_t(b[i+2])<<16u)|
        (std::uint32_t(b[i+3])<<24u);
}
static std::uint16_t le16(const std::vector<std::uint8_t>& b,
                          std::size_t i) noexcept {
    return std::uint16_t(b[i])|
        static_cast<std::uint16_t>(std::uint16_t(b[i+1])<<8u);
}

int main() {
    constexpr std::size_t frames=256u;
    std::array<std::int16_t,frames*2u> test{};
    for(std::size_t n=0;n<frames;++n) {
        test[n*2u]=static_cast<std::int16_t>(n*97u-14000);
        test[n*2u+1u]=static_cast<std::int16_t>(12000-n*63u);
    }
    test[0]=static_cast<std::int16_t>(-32768);
    test[1]=32767;
    const auto path=std::filesystem::temp_directory_path()/
        ("125A-P002-PRIVATE-PCM-WAV-SYNTHETIC-"+std::to_string(
            GetCurrentProcessId())+"-"+std::to_string(
            GetTickCount64())+".wav");
    CHECK(!std::filesystem::exists(path));
    using awe64::vst3::savePrivateNativeDryWav;
    CHECK(!savePrivateNativeDryWav(path,{},false));
    CHECK(!savePrivateNativeDryWav(path,test,true));
    CHECK(!std::filesystem::exists(path));
    CHECK(savePrivateNativeDryWav(path,test,false));
    std::ifstream stream(path,std::ios::binary);
    CHECK(stream);
    std::vector<std::uint8_t> b(
        (std::istreambuf_iterator<char>(stream)),{});
    const auto bytes=static_cast<std::uint32_t>(test.size()*2u);
    CHECK(b.size()==44u+bytes);
    CHECK(std::memcmp(b.data(),"RIFF",4)==0);
    CHECK(le32(b,4)==36u+bytes);
    CHECK(std::memcmp(b.data()+8,"WAVEfmt ",8)==0);
    CHECK(le32(b,16)==16u); // PCM fmt chunk
    CHECK(le16(b,20)==1u && le16(b,22)==2u); // PCM stereo
    CHECK(le32(b,24)==44100u);
    CHECK(le32(b,28)==176400u);
    CHECK(le16(b,32)==4u && le16(b,34)==16u);
    CHECK(std::memcmp(b.data()+36,"data",4)==0);
    CHECK(le32(b,40)==bytes);
    CHECK(le16(b,44)==0x8000u && le16(b,46)==0x7fffu);
    CHECK(std::memcmp(b.data()+44,test.data(),bytes)==0);
    // Native evidence is immutable, even when the caller retries.
    CHECK(!savePrivateNativeDryWav(path,test,false));
    CHECK(std::filesystem::file_size(path)==44u+bytes);
    // Win32 std::ifstream keeps a share-deny-delete handle open.
    // Closing is mandatory before removing the synthetic test file;
    // Linux unlink-while-open semantics do not apply on Windows.
    stream.close();
    CHECK(!stream.is_open());
    std::filesystem::remove(path);
    CHECK(!std::filesystem::exists(path));
    // No file for oversized 30-second buffer or an unwritable target.
    std::vector<std::int16_t> tooMany(44100u*2u*30u+2u);
    CHECK(!savePrivateNativeDryWav(path,tooMany,false));
    CHECK(!std::filesystem::exists(path));
    std::cout<<"P002_WIN32_NATIVE_DRY_PCM_WAV_PRECARD_44100_STEREO_16BIT_AND_NO_OVERWRITE_PASS\n";
    return 0;
}
