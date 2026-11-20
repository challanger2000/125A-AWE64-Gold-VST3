#pragma once
// Private per-instance 86Box profile: attach the optional, generated
// FAT16 SF2 data disk on IDE 0:1, never modify/replace the Windows95 disk.
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
namespace p007::sf2_profile {
namespace fs=std::filesystem;
inline bool configure(const fs::path& source,const fs::path& destination,
                      bool sf2Mounted,std::string& error) noexcept {
    try{
        std::ifstream in(source,std::ios::binary);
        if(!in){error="Missing original 86Box profile";return false;}
        std::string config((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
        if(config.size()>16384u||config.find("[Hard disks]")==std::string::npos||
           config.find("hdd_01_fn = win95.img")==std::string::npos||
           config.find("hdd_02_")!=std::string::npos) {
            error="Unexpected 86Box disk profile; refusing unsafe change";
            return false;
        }
        // Preserve original primary IDE 0:0 and every other 86Box setting.
        if(sf2Mounted) {
            const auto next=config.find("\n[",config.find("[Hard disks]"));
            if(next==std::string::npos) {
                error="Unterminated 86Box [Hard disks] section";return false;
            }
            const bool windowsNewlines=config.find("\r\n")!=std::string::npos;
            const std::string eol=windowsNewlines?"\r\n":"\n";
            const std::string second=
                eol+"hdd_02_fn = p007sf2.img"+eol+
                "hdd_02_parameters = 63, 16, 130, 0, ide"+eol+
                "hdd_02_ide_channel = 0:1"+eol+
                "hdd_02_speed = ramdisk"+eol;
            config.insert(next+1u,second);
        }
        fs::path staged=destination;staged+=L".p007-sf2config";
        std::error_code ec;
        if(fs::exists(staged,ec)){
            error="An earlier SF2 config transaction is unfinished";return false;
        }
        {
            std::ofstream out(staged,std::ios::binary|std::ios::trunc);
            if(!out||!out.write(config.data(),
                              static_cast<std::streamsize>(config.size()))){
                error="Could not write private 86Box SF2 profile";
                fs::remove(staged,ec);return false;
            }
        }
        // Only before the guest init: no emulator has opened this file yet.
        // The source template is immutable, as is win95.img.
        fs::rename(staged,destination,ec);
        if(ec){
            fs::remove(destination,ec);
            ec.clear();
            fs::rename(staged,destination,ec);
        }
        if(ec){error="Unable to activate guest SF2 profile";return false;}
        error.clear();return true;
    }catch(...){
        error="Unexpected failure preparing 86Box SF2 profile";
        return false;
    }
}
} // namespace p007::sf2_profile
