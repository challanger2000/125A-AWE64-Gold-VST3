// Adversarial Windows-only contract for the REAL private Win95 prelude.
// The actual owner Creative ROM/trace cannot be shipped into public CI.
#include "P002OriginalWin95ChipPrelude.h"
#include <iostream>
int main() {
    awe64::Chip chip;
    awe64::vst3::OriginalChipPreludeResult report;
    std::string error;
    if(chip.rebaseDiagnosticFrameClock())return 1;
    if(awe64::vst3::replayPrivateOriginalWin95ChipPrelude(
         chip,"nonexistent-original-win95-private-trace.txt",
         "nonexistent-original-win95-private-ram.bin",
         137,report,error))return 2;
    if(error.empty() || report.source_chip_clock_at_activation!=0u)return 3;
    std::cout<<"P002_ORIGINAL_WIN95_BOOT_PRELUDE_FAIL_CLOSED_WITHOUT_CHIP_PASS "
               "original_private_driver_evidence=NOT_IN_PUBLIC_CI\n";
    return 0;
}
