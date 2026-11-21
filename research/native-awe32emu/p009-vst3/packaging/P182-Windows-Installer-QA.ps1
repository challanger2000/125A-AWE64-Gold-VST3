# P182 — native Windows staging/atomic installer acceptance with 100% public
# synthetic assets only. This script and INSTALLIEREN.ps1 are public; never
# stage the owner's original licensed ROM/SBK or SoundFonts on GitHub runners.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = (Get-Location).Path
$fixture = Join-Path $env:RUNNER_TEMP ('awe64-p182-' + [Guid]::NewGuid().ToString('N'))
$package = Join-Path $fixture 'package'
$root = Join-Path $fixture 'isolated-common-files\VST3'
$bundle = 'AWE64_Gold_Native_P009.vst3'
$src = (Resolve-Path '_p009-build/VST3/Release/AWE64_Gold_Native_P009.vst3').Path
$to = Join-Path $package $bundle
try {
    New-Item -ItemType Directory -Path $package -Force | Out-Null
    Copy-Item -LiteralPath $src -Destination $to -Recurse -Force -ErrorAction Stop
    $assets = Join-Path $to 'Contents\Resources\P009-Private'
    New-Item -ItemType Directory -Path $assets -Force | Out-Null
    [IO.File]::WriteAllBytes((Join-Path $assets 'awe32.raw'), (New-Object byte[] 1048576))
    $py = Join-Path $repoRoot 'research/native-awe32emu/p009-vst3/tests/make_public_layered_sf2.py'
    & python $py (Join-Path $assets 'SYNTHGM.SBK') --wet
    if ($LASTEXITCODE -ne 0) { throw 'P182 synthetic SBK failed' }
    & python $py (Join-Path $assets 'user.sf2') --sustain
    if ($LASTEXITCODE -ne 0) { throw 'P182 synthetic user bank failed' }
    Copy-Item -LiteralPath (Join-Path $assets 'user.sf2') -Destination (Join-Path $assets '77-presets.sf2')
    & python $py (Join-Path $assets 'vintage.sf2') --alternate
    if ($LASTEXITCODE -ne 0) { throw 'P182 synthetic alternate bank failed' }
    $installer = Join-Path $repoRoot 'research/native-awe32emu/p009-vst3/packaging/INSTALLIEREN.ps1'
    Copy-Item -LiteralPath $installer -Destination (Join-Path $package 'INSTALLIEREN.ps1')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'research/native-awe32emu/p009-vst3/packaging/INSTALLIEREN.bat') -Destination $package
    [IO.File]::WriteAllText((Join-Path $package 'BITTE-LESEN.md'), 'PUBLIC_SYNTHETIC_TEST_ONLY')
    $entries = @(
        Get-ChildItem -LiteralPath $package -Recurse -File -Force |
        ForEach-Object {
            $relative = $_.FullName.Substring($package.Length + 1).Replace('\','/')
            @{
                path=$relative
                bytes=[long]$_.Length
                sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        }
    )
    $moduleRel = $bundle + '/Contents/x86_64-win/AWE64_Gold_Native_P009.vst3'
    $moduleSha = [string](@($entries | Where-Object { $_.path -eq $moduleRel })[0].sha256)
    if ($moduleSha -notmatch '^[0-9a-f]{64}$') { throw 'P182 module SHA could not be resolved' }
    $manifest = @{
        status='P182_CI_PUBLIC_SYNTHETIC_INSTALL_TEST_ONLY'
        source_sha=([string]$env:GITHUB_SHA)
        vst3_module_sha256=$moduleSha
        files=$entries
    }
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $package 'INTEGRITAET-SHA256.json') -Encoding UTF8
    $script = Join-Path $package 'INSTALLIEREN.ps1'
    # First real Windows PowerShell install: verified from complete manifest.
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script -DestinationRoot $root -NoElevation
    if ($LASTEXITCODE -ne 0) { throw 'P182 first Windows install failed' }
    $installed = Join-Path $root $bundle
    $destModule = Join-Path $installed 'Contents\x86_64-win\AWE64_Gold_Native_P009.vst3'
    if ((Get-FileHash -LiteralPath $destModule -Algorithm SHA256).Hash.ToLowerInvariant() -ne $moduleSha) {
        throw 'P182 actual installed Windows binary not bit-identical to CI VST3'
    }
    # The exact installed bundle must be a loadable Steinberg instrument.
    $validator = Get-ChildItem -LiteralPath (Join-Path $repoRoot '_p009-build') -Filter validator.exe -Recurse -File |
                 Where-Object { $_.FullName -match 'Release' } | Select-Object -First 1
    if (-not $validator) { throw 'P182 Steinberg validator missing' }
    $log = Join-Path $fixture 'installed-module-validator.log'
    & $validator.FullName $installed *> $log
    if ($LASTEXITCODE -ne 0 -or
        -not (Select-String -LiteralPath $log -Pattern 'Result: +47 tests passed, 0 tests failed' -Quiet)) {
        Get-Content -LiteralPath $log | Select-Object -Last 35
        throw 'P182 installed real VST3 module failed 47 Steinberg validations'
    }
    Write-Host 'P182_INSTALLED_ACTUAL_WINDOWS_VST3_STEINBERG_47_OF_47_PASS'
    # Second install: old module is not silently overwritten or deleted.
    $sentinel = Join-Path $installed 'P182-old-version-sentinel.txt'
    [IO.File]::WriteAllText($sentinel, 'THIS_PREVIOUS_PLUGIN_MUST_BE_BACKED_UP')
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script -DestinationRoot $root -NoElevation
    if ($LASTEXITCODE -ne 0) { throw 'P182 second Windows install failed' }
    $backups = @(Get-ChildItem -LiteralPath $root -Directory -Filter ($bundle + '.backup-*'))
    if ($backups.Count -ne 1 -or -not (Test-Path -LiteralPath (Join-Path $backups[0].FullName 'P182-old-version-sentinel.txt')) -or
        (Test-Path -LiteralPath (Join-Path $installed 'P182-old-version-sentinel.txt'))) {
        throw 'P182 previous plugin was not atomically backed up'
    }
    # P186: simulate a first installer holding the same interprocess lock.
    # A failed overlapping second process must not remove/rollback the
    # successfully installed bundle (Codex P185 NO-GO counterexample).
    $lockPath = Join-Path $root ($bundle + '.install.lock')
    $exclusive = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate,
        [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        $raceOut = Join-Path $fixture 'overlap-out.log'
        $raceErr = Join-Path $fixture 'overlap-error.log'
        $raceArgs = '-NoProfile -ExecutionPolicy Bypass -File "' + $script +
                    '" -DestinationRoot "' + $root + '" -NoElevation'
        $racer = Start-Process -FilePath 'powershell.exe' -ArgumentList $raceArgs -Wait -PassThru -RedirectStandardOutput $raceOut -RedirectStandardError $raceErr
        if ($racer.ExitCode -eq 0 -or
            (Get-Content -LiteralPath $raceErr -Raw) -notmatch 'Installation laeuft bereits') {
            throw 'P186 overlapping installer was accepted or failed for unrelated reason'
        }
        if ((Get-FileHash -LiteralPath $destModule -Algorithm SHA256).Hash.ToLowerInvariant() -ne $moduleSha -or
            @(Get-ChildItem -LiteralPath $root -Directory -Filter ($bundle + '.backup-*')).Count -ne 1 -or
            @(Get-ChildItem -LiteralPath $root -Directory -Filter ($bundle + '.stage-*')).Count -ne 0) {
            throw 'P186 failed overlapping installer mutated a committed target/backup'
        }
        Write-Host 'P186_OVERLAPPING_INSTALLER_FAILS_CLOSED_EXISTING_BUNDLE_PRESERVED_PASS'
    } finally {
        $exclusive.Dispose()
    }
    # P188 INDEPENDENT PROCESS-KILL: actual Windows second installer
    # reaches the post-rename/pre-publish window with a fully validated
    # stage and a separate already-ready recovery watchdog. We kill the
    # installer *process*, not simulate a catch/throw. The previous bundle
    # and the older historical backup must survive byte-identically.
    $crashSentinel = Join-Path $installed 'P188-current-bundle-survives-kill.txt'
    [IO.File]::WriteAllText($crashSentinel, 'PREVIOUS_COMPLETE_BUNDLE_AFTER_FORCED_KILL')
    $beforeCrash = @{}
    Get-ChildItem -LiteralPath $installed -Recurse -File -Force |
        ForEach-Object {
            $key = $_.FullName.Substring($installed.Length + 1)
            $beforeCrash[$key] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        }
    $historical = @(
        Get-ChildItem -LiteralPath $root -Directory -Filter ($bundle + '.backup-*')
    )
    if ($historical.Count -ne 1) { throw 'P188 missing prior historical backup' }
    $priorBackup = $historical[0].FullName
    $priorSentinel = Join-Path $priorBackup 'P182-old-version-sentinel.txt'
    $oldHistoricalHash = (Get-FileHash -LiteralPath $priorSentinel -Algorithm SHA256).Hash
    $crashOut = Join-Path $fixture 'p188-forced-kill-out.log'
    $crashErr = Join-Path $fixture 'p188-forced-kill-error.log'
    $crashArgs = '-NoProfile -ExecutionPolicy Bypass -File "' + $script +
                 '" -DestinationRoot "' + $root + '" -NoElevation'
    $victim = $null
    $env:P188_INSTALLER_CI_KILLPOINT = '1'
    try {
        $victim = Start-Process -FilePath 'powershell.exe' -ArgumentList $crashArgs -PassThru -RedirectStandardOutput $crashOut -RedirectStandardError $crashErr
        $until = [DateTime]::UtcNow.AddSeconds(90)
        $observedWindow = $false
        while ([DateTime]::UtcNow -lt $until) {
            $signals = @(
                Get-ChildItem -LiteralPath $root -File -Filter ($bundle + '.killpoint-*')
            )
            if ($signals.Count -eq 1) {
                if (Test-Path -LiteralPath $installed) {
                    throw 'P188 forced-kill marker appeared while canonical target still existed'
                }
                $observedWindow = $true
                break
            }
            $victim.Refresh()
            if ($victim.HasExited) {
                throw ('P188 victim exited before killpoint: ' + $victim.ExitCode +
                       ' ' + (Get-Content -LiteralPath $crashErr -Raw))
            }
            Start-Sleep -Milliseconds 30
        }
        if (-not $observedWindow) {
            throw 'P188 actual installer never reached destructive rename gap'
        }
        Stop-Process -Id $victim.Id -Force -ErrorAction Stop
        if (-not $victim.WaitForExit(10000)) {
            throw 'P188 real installer process did not terminate'
        }
    } finally {
        Remove-Item Env:\P188_INSTALLER_CI_KILLPOINT -ErrorAction SilentlyContinue
        if ($null -ne $victim) {
            $victim.Refresh()
            if (-not $victim.HasExited) {
                Stop-Process -Id $victim.Id -Force -ErrorAction SilentlyContinue
            }
            $victim.Dispose()
        }
    }
    # Watchdog must repair the missing canonical path WITHOUT a new
    # installer invocation. This is different from catch/finally rollback.
    $repairDeadline = [DateTime]::UtcNow.AddSeconds(45)
    $repaired = $false
    while ([DateTime]::UtcNow -lt $repairDeadline) {
        $journals = @(
            Get-ChildItem -LiteralPath $root -File -Filter ($bundle + '.txn-*.journal')
        )
        if ((Test-Path -LiteralPath $installed -PathType Container) -and
            $journals.Count -eq 0) {
            $repaired = $true
            break
        }
        Start-Sleep -Milliseconds 50
    }
    if (-not $repaired) {
        throw 'P188 CRASH: no automatic recovery of canonical bundle after process kill'
    }
    $afterCrashFiles = @(Get-ChildItem -LiteralPath $installed -Recurse -File -Force)
    if ($afterCrashFiles.Count -ne $beforeCrash.Count) {
        throw 'P188 watchdog restored missing or additional files'
    }
    foreach ($f in $afterCrashFiles) {
        $key = $f.FullName.Substring($installed.Length + 1)
        if (-not $beforeCrash.ContainsKey($key) -or
            (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash -ne $beforeCrash[$key]) {
            throw ('P188 watchdog did not restore original byte-exact file: ' + $key)
        }
    }
    if (-not (Test-Path -LiteralPath $priorSentinel -PathType Leaf) -or
        (Get-FileHash -LiteralPath $priorSentinel -Algorithm SHA256).Hash -ne $oldHistoricalHash -or
        @(Get-ChildItem -LiteralPath $root -Directory -Filter ($bundle + '.backup-*')).Count -ne 1 -or
        @(Get-ChildItem -LiteralPath $root -Directory -Filter ($bundle + '.stage-*')).Count -ne 0 -or
        @(Get-ChildItem -LiteralPath $root -File -Filter ($bundle + '.killpoint-*')).Count -ne 0) {
        throw 'P188 forced crash damaged previous backup or left orphan stage/marker'
    }
    Write-Host 'P188_WINDOWS_FORCED_PROCESS_KILL_WATCHDOG_RESTORED_BYTE_EXACT_PREVIOUS_BUNDLE_PASS'

    # Independent fault injection on two disposable destinations. The
    # ordinary watchdog gets no test override and must fail closed.
    $publishWrapper = Join-Path $fixture 'pause-after-publication.ps1'
    @'
param([string]$Installer,[string]$DestinationRoot,[string]$PublishMarker)
$ErrorActionPreference = 'Stop'
function Move-Item {
    [CmdletBinding()]
    param([string]$LiteralPath,[string]$Destination)
    Microsoft.PowerShell.Management\Move-Item @PSBoundParameters
    if ($LiteralPath -like '*.stage-*') {
        [IO.File]::WriteAllText($PublishMarker, 'P189_AFTER_STAGE_RENAME')
        Start-Sleep -Seconds 60
    }
}
& $Installer -DestinationRoot $DestinationRoot -NoElevation
'@ | Set-Content -LiteralPath $publishWrapper -Encoding UTF8

    function Wait-P189([scriptblock]$condition) {
        $until = [DateTime]::UtcNow.AddSeconds(45)
        while ([DateTime]::UtcNow -lt $until) {
            if (& $condition) { return }
            Start-Sleep -Milliseconds 30
        }
        throw 'P189 process did not reach the required recovery phase.'
    }

    foreach ($variant in @('backup','target')) {
        $testRoot = Join-Path $fixture ('p189-' + $variant)
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script -DestinationRoot $testRoot -NoElevation
        if ($LASTEXITCODE -ne 0) { throw 'P189 failed to prepare the previous bundle' }
        $target = Join-Path $testRoot $bundle
        [IO.File]::WriteAllText((Join-Path $target 'p189-old-sentinel.txt'), 'ORIGINAL-SHA')
        $pause = Join-Path $fixture ('p189-' + $variant + '.pause')
        $victimOut = Join-Path $fixture ('p189-' + $variant + '.out')
        $victimErr = Join-Path $fixture ('p189-' + $variant + '.err')
        $victim = $null
        if ($variant -eq 'backup') {
            $env:P188_INSTALLER_CI_KILLPOINT = '1'
            $arg = '-NoProfile -ExecutionPolicy Bypass -File "' + $script +
                   '" -DestinationRoot "' + $testRoot + '" -NoElevation'
        } else {
            $arg = '-NoProfile -ExecutionPolicy Bypass -File "' + $publishWrapper +
                   '" -Installer "' + $script + '" -DestinationRoot "' + $testRoot +
                   '" -PublishMarker "' + $pause + '"'
        }
        try {
            $victim = Start-Process -FilePath 'powershell.exe' -ArgumentList $arg -PassThru -RedirectStandardOutput $victimOut -RedirectStandardError $victimErr
            Wait-P189 {
                if ($victim.HasExited) { throw ('P189 victim exited: ' + (Get-Content -LiteralPath $victimErr -Raw)) }
                if ($variant -eq 'target') { Test-Path -LiteralPath $pause }
                else { @(Get-ChildItem -LiteralPath $testRoot -File -Filter ($bundle + '.killpoint-*')).Count -eq 1 }
            }
            $old = @(Get-ChildItem -LiteralPath $testRoot -Directory -Filter ($bundle + '.backup-*'))
            if ($old.Count -ne 1 -or (Test-Path -LiteralPath $target) -ne ($variant -eq 'target')) {
                throw 'P189 test did not reach the intended directory state.'
            }
            $edited = if ($variant -eq 'backup') { $old[0].FullName } else { $target }
            $damaged = Join-Path $edited 'Contents\Resources\moduleinfo.json'
            $corrupt = [IO.File]::ReadAllBytes($damaged)
            $corrupt[0] = $corrupt[0] -bxor 1
            [IO.File]::WriteAllBytes($damaged, $corrupt)
            $damageSha = (Get-FileHash -LiteralPath $damaged -Algorithm SHA256).Hash
            Stop-Process -Id $victim.Id -Force -ErrorAction Stop
            if (-not $victim.WaitForExit(10000)) { throw 'P189 failed to kill test installer' }
        } finally {
            Remove-Item Env:\P188_INSTALLER_CI_KILLPOINT -ErrorAction SilentlyContinue
            if ($null -ne $victim) {
                if (-not $victim.HasExited) { Stop-Process -Id $victim.Id -Force -ErrorAction SilentlyContinue }
                $victim.Dispose()
            }
        }
        Wait-P189 {
            @(Get-ChildItem -LiteralPath $testRoot -File -Filter '*.failure' |
                Where-Object { $_.Length -gt 0 }).Count -eq 1
        }
        $failure = @(Get-ChildItem -LiteralPath $testRoot -File -Filter '*.failure')[0]
        $reason = if ($variant -eq 'backup') { 'backup is missing or its tree SHA256' } else { 'target tree does not match' }
        if ((Get-Content -LiteralPath $failure.FullName -Raw) -notmatch $reason -or
            @(Get-ChildItem -LiteralPath $testRoot -File -Filter '*.journal').Count -ne 1 -or
            (Get-FileHash -LiteralPath $damaged -Algorithm SHA256).Hash -ne $damageSha -or
            (Get-Content -LiteralPath (Join-Path $old[0].FullName 'p189-old-sentinel.txt') -Raw) -ne 'ORIGINAL-SHA' -or
            (Test-Path -LiteralPath $target) -ne ($variant -eq 'target')) {
            throw 'P189 corrupt tree was published, removed or declared recovered.'
        }
        if ($variant -eq 'backup' -and @(Get-ChildItem -LiteralPath $testRoot -Directory -Filter ($bundle + '.stage-*')).Count -ne 1) {
            throw 'P189 corrupt backup case lost its intact staging tree.'
        }
        Write-Host ('P189_NATIVE_INTERRUPTED_' + $variant.ToUpperInvariant() + '_INTEGRITY_FAIL_CLOSED_PASS')
    }

    # Ensure no stale mutex prevents a valid subsequent install. The ordinary
    # tampered-source negative case below continues to test rollback integrity.
    # The negative control MUST fail before any replacement on corrupted SF2.
    # Neither the installed module nor its one existing backup may change.
    $user = Join-Path $assets 'user.sf2'
    [IO.File]::WriteAllBytes($user, [byte[]](0x11,0x22,0x33,0x44))
    # A genuine corrupted source deliberately makes powershell.exe write
    # to stderr. Capture its EXPECTED failure without converting native
    # stderr to a pwsh ErrorActionPreference=Stop terminating exception.
    $badOut = Join-Path $fixture 'tampered-install-out.log'
    $badErr = Join-Path $fixture 'tampered-install-error.log'
    $badArgs = '-NoProfile -ExecutionPolicy Bypass -File "' + $script +
               '" -DestinationRoot "' + $root + '" -NoElevation'
    $bad = Start-Process -FilePath 'powershell.exe' -ArgumentList $badArgs -Wait -PassThru -RedirectStandardOutput $badOut -RedirectStandardError $badErr
    if ($bad.ExitCode -eq 0) {
        throw 'P182 deliberately corrupted SoundFont was accepted by installer'
    }
    if ((Get-Content -LiteralPath $badErr -Raw) -notmatch
        'Dateigroesse falsch|SHA256-Pruefung fehlgeschlagen') {
        throw 'P182 corrupt-SF2 negative control failed for unrelated reason'
    }
    Write-Host ('P182_EXPECTED_CORRUPT_SF2_INSTALL_REJECTED_EXIT=' + $bad.ExitCode)
    $after = @(Get-ChildItem -LiteralPath $root -Directory -Filter ($bundle + '.backup-*'))
    $stages = @(Get-ChildItem -LiteralPath $root -Directory -Filter ($bundle + '.stage-*'))
    if ($after.Count -ne 1 -or $stages.Count -ne 0 -or
        (Get-FileHash -LiteralPath $destModule -Algorithm SHA256).Hash.ToLowerInvariant() -ne $moduleSha) {
        throw 'P182 a failed preflight modified/destroyed installed bundle or backup'
    }
    Write-Host 'P182_REAL_WIN64_ATOMIC_INSTALL_REINSTALL_TAMPER_REJECT_BACKUP_PASS'
} finally {
    if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
}
