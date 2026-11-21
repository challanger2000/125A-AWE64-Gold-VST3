# P188 — Windows installer forced-kill recovery (Codex P186 X07)

## Historical release blocker

The independent P186 Codex X07 counterexample killed the private Win64 installer **after** it renamed the existing canonical `AWE64_Gold_Native_P009.vst3` to its backup and **before** the fully verified staged replacement was published. The canonical VST3 folder remained missing; the previous bundle still existed, byte-complete, in a backup and the new stage was left over. A mutex, catch/finally, and green SHA256 installer CI **cannot** repair an abruptly killed process.

The P187 source `dfe12ad144b2fbe2c10dd71f9c87f30aa0249d98` is a **successful targeted Windows build** (Steinberg 47/47; both Plugin Testers 41/0/0; 1,182,750 sounded controller-echo saves with 0 stale, mixed, rejected). That is **not** evidence that X07 is fixed. Current PR #46 remains DRAFT and release NO-GO.

## P188 change (no VST3 product/EMU8000 code changes)

The normal PowerShell installer now has an independent **short-lived recovery watchdog mode**. For an update of an existing bundle it:

1. Obtains the existing per-destination nonshareable Windows lock, copies the complete new private bundle to a same-volume stage, and verifies every file against the owner-owned package manifest by SHA256.
2. Writes a GUID-bound transaction journal in the same parent directory and starts a hidden independent `powershell.exe` watchdog executing a recovery-only path of the **same signed-by-manifest installer script**. The parent requires the watchdog's explicit ready handshake **before** renaming the old canonical directory. Failure to launch the watcher aborts the update with the old canonical bundle still in place.
3. Renames the complete old canonical directory to a GUID-specific backup, then renames the preverified stage to the canonical path, verifies it once more, and releases the exclusive mutex.
4. Once the parent has exited or been forcibly killed, the watchdog acquires that same mutex (the OS releases the parent process's FileStream on termination) and examines its GUID-bound journal. If the canonical folder is missing but the matching backup exists, it restores the **entire original folder by a single same-volume rename**, then cleans its abandoned stage and journal. If the canonical folder already exists, it does not overwrite it or touch any historical backup.
5. A later normal installation also scans and restores any validated abandoned journals **while holding the same lock**, including cases where both installer and watchdog disappeared after machine shutdown. No unrelated `backup-*` folder can be recovered or deleted without an exact valid P188 journal.

This is an **automatic process-crash recovery** mechanism, not a verified atomic zero-gap Windows directory exchange. A small interval of canonical unavailability still exists between directory renames and, after an abrupt kill, until the independent watcher restores it. The watcher also cannot guarantee recovery after a simultaneous shutdown/power loss of both Windows processes before the next installation attempt. A nonempty-directory replacement with `Move-Item` on Windows is not a documented atomic exchange operation. Therefore **strict uninterrupted canonical availability is still NOT VERIFIED**; do not claim otherwise.

## Windows negative oracle

`P182-Windows-Installer-QA.ps1` creates only public synthetic 86Box/ROM/SBK/SF2 fixtures and real compiled MSVC x64 VST3 module. It installs and verifies the module through the Steinberg Validator and creates a separate pre-existing historical backup.

The new **real two-process test** adds a unique extra sentinel file to the currently installed canonical VST3 and hashes **all** its files (including the extra sentinel). It launches a second real `powershell.exe` installer with the confined CI-only `P188_INSTALLER_CI_KILLPOINT=1`. This test-only hook pauses **only after** the original directory has truly been moved to the new transaction backup and the independent watchdog has acknowledged readiness. The test verifies the canonical directory is genuinely absent at that instant, then forcibly kills the installer with `Stop-Process -Force` (no PowerShell catch/finally). Without invoking another installer, it observes the independent watchdog restore the previous canonical folder. Every file path, SHA256 and count must match the old full bundle; the older historical backup SHA and untouched size are checked; there must be no stage, journal or orphan killpoint. The existing two-process mutex collision and deliberate tampered-SF2 rejection gates remain in the suite.

Expected new marker: `P188_WINDOWS_FORCED_PROCESS_KILL_WATCHDOG_RESTORED_BYTE_EXACT_PREVIOUS_BUNDLE_PASS`.

The source/CI is deliberately red until this exact negative is demonstrated on the Windows runner. A mere passing Steinberg validator or ordinary installer test is **insufficient**.

## Mandatory remaining verification

- Actual Win64 process-kill negative and deterministic race controls on exact P188 SHA.
- Repeat-run crash timing, shutdown/watcher-death recovery, stale/journal corruption, UAC/restricted file-system shares, ACLs and Windows 11 system process-kill.
- Independent Codex original X07 exact counterexample, original N08 and X01–X06 unchanged scripts, open-source license and remaining code review.
- Private Creative ROM + SYNTHGM.SBK + owner 77-SF2, real hardware and Fender Studio Pro/ASIO acceptance.
- **No end-user install package or release.** PR #46 stays DRAFT/NO-GO until the independent audit clears the applicable contracts.
