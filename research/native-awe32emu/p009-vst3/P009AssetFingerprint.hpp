#pragma once
// Owner-only SoundFont file identity. Non-realtime ONLY (GUI/host state).
// Fail CLOSED on missing/changed asset. No SF2 bytes enter the public repo,
// project state or GitHub. BCrypt is included with Windows 10/11.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace p009 {
inline bool sha256FileNonRealtime(const std::filesystem::path& path,
                                  std::array<std::uint8_t,32>& result) {
    std::error_code pathError;
    if(!std::filesystem::is_regular_file(path,pathError) || pathError)
        return false;
    BCRYPT_ALG_HANDLE provider=nullptr;
    BCRYPT_HASH_HANDLE handle=nullptr;
    DWORD bytes=0,objectBytes=0;
    if(!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(
        &provider,BCRYPT_SHA256_ALGORITHM,nullptr,0)))return false;
    const auto close=[&](){
        if(handle) BCryptDestroyHash(handle);
        if(provider) BCryptCloseAlgorithmProvider(provider,0);
    };
    if(!BCRYPT_SUCCESS(BCryptGetProperty(provider,BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&objectBytes),sizeof(objectBytes),
        &bytes,0)) || objectBytes==0 || objectBytes>32768) {
        close();return false;
    }
    std::array<std::uint8_t,32768> object{};
    if(!BCRYPT_SUCCESS(BCryptCreateHash(provider,&handle,object.data(),
        objectBytes,nullptr,0,0))) {
        close();return false;
    }
    std::ifstream f;
    try {
        f.open(path,std::ios::binary);
    }catch(...){
        close();return false;
    }
    if(!f){close();return false;}
    std::array<char,65536> buf{};
    while(f){
        f.read(buf.data(),static_cast<std::streamsize>(buf.size()));
        const auto n=f.gcount();
        if(n>0 && !BCRYPT_SUCCESS(BCryptHashData(handle,
           reinterpret_cast<PUCHAR>(buf.data()),static_cast<ULONG>(n),0))){
            close();return false;
        }
    }
    if(!f.eof()){close();return false;}
    std::array<std::uint8_t,32> candidate{};
    const bool okay=BCRYPT_SUCCESS(BCryptFinishHash(handle,
        candidate.data(),static_cast<ULONG>(candidate.size()),0));
    close();
    if(okay)result=candidate;
    return okay;
}
} // namespace p009
