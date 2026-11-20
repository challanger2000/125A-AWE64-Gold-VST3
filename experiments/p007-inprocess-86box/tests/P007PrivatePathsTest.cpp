#include "P007PrivatePaths.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <cassert>
#include <cstdio>
#include <string>
namespace fs=std::filesystem;
static int fail(int code) { std::fprintf(stderr,"P007_PRIVATE_PATHS_FAIL_CODE=%d\n",code); return code; }
static void write(const fs::path& p,const char* text) {
    std::ofstream out(p,std::ios::binary);
    out<<text;
    assert(out.good());
}
int main() {
    wchar_t temp[MAX_PATH]{};
    if(!GetTempPathW(MAX_PATH,temp)) return fail(10);
    const fs::path base=fs::path(temp)/
        (L"125A-P007-private-test-"+std::to_wstring(GetCurrentProcessId()));
    std::error_code ec;
    fs::create_directories(base,ec);
    if(ec)return fail(11);
    const fs::path core=base/L"OnePlugin.vst3"/L"Contents"/L"x86_64-win"/L"125A_AWE_Core.dll";
    const fs::path bundle=p007::privatevm::bundleRoot(core);
    if(bundle!=base/L"OnePlugin.vst3"/L"Contents"/L"Resources"/L"PrivateVM")return fail(12);
    const fs::path writable=base/L"WritableVM";
    if(p007::privatevm::prepare(bundle,writable,ec))return fail(13);
    fs::create_directories(bundle/L"vm");
    fs::create_directories(bundle/L"roms");
    write(bundle/L"vm"/L"P007_AWE64.cfg","guest-config");
    write(bundle/L"86box.cfg","global");
    // Config file alone is NOT a complete bootable original Windows95 VM.
    if(p007::privatevm::prepare(bundle,writable,ec))return fail(21);
    write(bundle/L"vm"/L"win95.img","original guest disk template");
    if(!p007::privatevm::prepare(bundle,writable,ec)){
        std::fprintf(stderr,"P007_PRIVATE_PATHS_PREPARE_ERROR=%s\\n",ec.message().c_str());
        return fail(14);
    }
    if(!p007::privatevm::complete(writable))return fail(15);
    write(writable/L"vm"/L"win95.img","guest modified after boot");
    if(!p007::privatevm::prepare(bundle,writable,ec))return fail(16);
    std::ifstream disk(writable/L"vm"/L"win95.img");
    std::string persisted;
    std::getline(disk,persisted);
    if(persisted!="guest modified after boot")return fail(17);
    disk.close(); // Windows will not delete an image still open by a handle.
    fs::remove(writable/L"vm"/L"win95.img",ec);
    if(ec)return fail(22);
    if(p007::privatevm::complete(writable))return fail(23);
    // Preserve incomplete persistent VM: do not silently restore from
    // bundled template because it would erase the user's emulated state.
    if(p007::privatevm::prepare(bundle,writable,ec))return fail(24);
    if(!fs::is_regular_file(writable/L".p007-installed-v1",ec))return fail(25);
    const fs::path incomplete=base/L"IncompleteGuest";
    fs::create_directories(incomplete);
    write(incomplete/L"my-disk.img","preserve me");
    if(p007::privatevm::prepare(bundle,incomplete,ec))return fail(18);
    if(!fs::is_regular_file(incomplete/L"my-disk.img"))return fail(19);
    fs::remove_all(base,ec);
    if(ec)return fail(20);
    std::puts("P007_BUNDLED_VM_RESOLUTION_AND_WRITABLE_PRIVATE_COPY_PASS");
    return 0;
}
