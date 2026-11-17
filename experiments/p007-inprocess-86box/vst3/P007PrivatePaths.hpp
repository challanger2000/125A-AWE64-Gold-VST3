#pragma once
#include <filesystem>
#include <fstream>
#include <system_error>

namespace p007::privatevm {
namespace fs=std::filesystem;
inline fs::path bundleRoot(const fs::path& coreDll) {
    return (coreDll.parent_path().parent_path()/L"Resources"/L"PrivateVM").lexically_normal();
}
inline bool complete(const fs::path& target) {
    std::error_code ec;
    return fs::is_regular_file(target/L".p007-installed-v1",ec)
        && fs::is_directory(target/L"vm",ec)
        && fs::is_regular_file(target/L"vm"/L"P007_AWE64.cfg",ec)
        && fs::is_regular_file(target/L"vm"/L"win95.img",ec)
        && fs::is_regular_file(target/L"86box.cfg",ec);
}
inline bool prepare(const fs::path& bundle,const fs::path& target,std::error_code& ec) {
    ec.clear();
    if (!fs::is_directory(bundle/L"vm",ec) || !fs::is_directory(bundle/L"roms",ec)
        || !fs::is_regular_file(bundle/L"vm"/L"P007_AWE64.cfg",ec)
        || !fs::is_regular_file(bundle/L"vm"/L"win95.img",ec)
        || !fs::is_regular_file(bundle/L"86box.cfg",ec)) return false;
    if(fs::exists(target,ec)) return complete(target);
    fs::create_directories(target.parent_path(),ec);
    if(ec) return false;
    // An interrupted copy is NOT discarded automatically. Never delete a
    // potentially valuable guest HDD or overwrite a modified user image.
    fs::path stage=target; stage+=L".staging";
    if(fs::exists(stage,ec)) return false;
    fs::create_directories(stage,ec);
    if(ec) return false;
    fs::copy(bundle/L"vm",stage/L"vm",fs::copy_options::recursive,ec);
    if(ec) return false;
    fs::copy_file(bundle/L"86box.cfg",stage/L"86box.cfg",ec);
    if(ec) return false;
    {
        std::ofstream marker(stage/L".p007-installed-v1",std::ios::binary|std::ios::trunc);
        if(!marker) return false;
        marker<<"P007_PRIVATE_PROFILE_V1\n";
        if(!marker) return false;
    }
    fs::rename(stage,target,ec);
    return !ec && complete(target);
}
} // namespace p007::privatevm
