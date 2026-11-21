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
        source_sha='4025170e89b36a17aafc66e1f4b8266e3dc7c055'
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
