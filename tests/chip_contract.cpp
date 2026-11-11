#include "Chip.h"
#include <86box/sound.h>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#define REQUIRE(condition) do { if (!(condition)) { \
    std::cerr << "FAIL " #condition " at line " << __LINE__ << '\n'; return EXIT_FAILURE; \
} } while (false)

int main() {
    using namespace awe64;
    const auto rom = std::filesystem::temp_directory_path() / "125a-p001-generated-test-rom.raw";
    // Purely synthetic ROM fixture; never advertised as Creative sound.
    { std::ofstream out(rom, std::ios::binary | std::ios::trunc);
      REQUIRE(static_cast<bool>(out));
      const std::array<char, 4096> zeros{};
      for (int i=0;i<256;++i) out.write(zeros.data(), zeros.size());
      REQUIRE(static_cast<bool>(out)); }
    Chip chip;
    std::string error;
    if (!chip.open(rom,error)) { std::cerr << "FAIL chip.open: " << error << "\\n"; return EXIT_FAILURE; }
    REQUIRE(error.empty());
    REQUIRE(chip.ready());
    const auto* p=chip.rawForContractTesting();
    REQUIRE(p->rom && p->ram && p->empty);
    REQUIRE(p->ram_end_addr == 0x1000000u);
    REQUIRE(p->ram_pointers[7] == p->rom + 7u*65536u);
    REQUIRE(p->ram_pointers[8] == p->empty);
    REQUIRE(p->ram_pointers[31] == p->empty);
    REQUIRE(p->ram_pointers[32] == p->ram);
    REQUIRE(p->ram_pointers[255] == p->ram + 223u*65536u);
    REQUIRE(p->ram_pointers[232] == p->ram + 200u*65536u);

    const std::array<int16_t,3> three = {123, -123, 456};
    REQUIRE(chip.uploadRam(kSampleRamBase, three));
    REQUIRE(p->ram[0] == 123 && p->ram[1] == -123 && p->ram[2] == 456);
    const std::array<int16_t,1> last = {27182};
    REQUIRE(chip.uploadRam(kFmReservedBase-1,last));
    REQUIRE(p->ram[(kFmReservedBase-1)-kSampleRamBase] == 27182);
    REQUIRE(!chip.uploadRam(kSampleRamBase-1,last));
    REQUIRE(!chip.uploadRam(kFmReservedBase,last));
    REQUIRE(!chip.uploadRam(kFmReservedBase-1,three));
    REQUIRE(p->ram[(kFmReservedBase-1)-kSampleRamBase] == 27182);
    REQUIRE(p->ram[0] == 123);

    // Actual EMU8000 port I/O path, not only a C++ memcpy or pointer test:
    // SMALW (reg 1, voice 22) controls the DRAM word write pointer.
    // SMLD (reg 1, voice 26) writes a sample and increments that pointer.
    const auto setRamPointer=[&](unsigned v, uint32_t address) {
        chip.writePort(0xE22, uint16_t((1u<<5)|v));
        chip.writePort(0xA20, uint16_t(address));
        chip.writePort(0xA22, uint16_t(address>>16));
    };
    setRamPointer(22u, 0x200000u);
    chip.writePort(0xE22, 0x3Au);
    chip.writePort(0xA20, 0xABCDu);
    REQUIRE(uint16_t(p->ram[0])==0xABCDu);
    setRamPointer(22u, 0xFFFFDFu);
    chip.writePort(0xE22, 0x3Au);
    chip.writePort(0xA20, 0x55AAu);
    REQUIRE(uint16_t(p->ram[0xFFFFDFu-0x200000u])==0x55AAu);
    chip.writePort(0xA20, 0xDEADu); // next address is reserved FM window
    REQUIRE(uint16_t(p->ram[0xFFFFDFu-0x200000u])==0x55AAu);
    setRamPointer(22u, 0x1FFFFFu);  // immediately below installed RAM
    chip.writePort(0xE22, 0x3Au);
    chip.writePort(0xA20, 0xBEEFu);
    REQUIRE(uint16_t(p->ram[0])==0xABCDu);
    // SMALR (reg 1, voice 20), followed by the original pipelined SMLD read.
    setRamPointer(20u, 0x200000u);
    chip.writePort(0xE22, 0x3Au);
    (void)chip.readPort(0xA20); // first fetch primes SMRD buffer
    REQUIRE(chip.readPort(0xA20)==0xABCDu);
    std::cout << "P001 original-EMU8K RAM port DMA first/last/reserved/readback PASS\n";

    // Runtime memory is backed by 28 MiB, not a GUI-label approximation.
    REQUIRE(kSampleRamBytes == 29360128u);
    REQUIRE(kSampleRamKiB == 28672);

    // Split-render and full-render results are identical after fresh init.
    std::array<int32_t, 2u*128u> a{}, b{};
    chip.render(a.data(),128);
    chip.close();
    if (!chip.open(rom,error)) { std::cerr << "FAIL chip.open: " << error << "\\n"; return EXIT_FAILURE; }
    chip.render(b.data(),31);
    chip.render(b.data()+62,97);
    REQUIRE(a == b);
    chip.close();
    // Negative: preflight rejects corrupt ROM, never calls fatal/plays fallback.
    { std::ofstream out(rom,std::ios::binary|std::ios::trunc); out.put('X'); }
    REQUIRE(!chip.open(rom,error));
    REQUIRE(!error.empty());
    REQUIRE(!chip.ready());
    std::filesystem::remove(rom);
    std::cout << "P001 chip init / 28-MiB address map / ROM rejection / block partition PASS\n";
    std::cout << "NOTE: synthetic ROM is NOT a fork digital-PCM acceptance reference.\n";
    return EXIT_SUCCESS;
}
