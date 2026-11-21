# 125A AWE64 Gold — private Windows VST3 installation
# The complete bundle and its full SHA256 manifest are verified BEFORE
# replacing an existing instrument. Never install a partial directory.
[CmdletBinding()]
param(
    [string]$DestinationRoot = '',
    [switch]$NoElevation,
    [switch]$RecoveryWatchdog,
    [string]$RecoveryToken = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$bundleName = 'AWE64_Gold_Native_P009.vst3'
# P188 short-lived independent recovery process. The ordinary installer
# must receive a ready handshake before displacing the existing bundle.
# The OS releases its exclusive lock if the process is terminated; this
# helper then restores the previous WHOLE directory, never a partial copy.
# This is recovery, not an unsupported claim of an atomic directory swap.
if ($RecoveryWatchdog) {
    if ([string]::IsNullOrWhiteSpace($DestinationRoot) -or
        $RecoveryToken -cnotmatch '^[0-9a-f]{32}$') {
        throw 'P188 recovery watchdog arguments invalid.'
    }
    $watchRoot = [IO.Path]::GetFullPath($DestinationRoot)
    $watchTarget = Join-Path $watchRoot $bundleName
    $watchStage = Join-Path $watchRoot ($bundleName + '.stage-' + $RecoveryToken)
    $watchBackup = Join-Path $watchRoot ($bundleName + '.backup-' + $RecoveryToken)
    $watchJournal = Join-Path $watchRoot ($bundleName + '.txn-' + $RecoveryToken + '.journal')
    $watchReady = Join-Path $watchRoot ($bundleName + '.txn-' + $RecoveryToken + '.ready')
    $watchKillPoint = Join-Path $watchRoot ($bundleName + '.killpoint-' + $RecoveryToken)
    $watchLockPath = Join-Path $watchRoot ($bundleName + '.install.lock')
    if (-not (Test-Path -LiteralPath $watchJournal -PathType Leaf) -or
        [IO.File]::ReadAllText($watchJournal) -cne ('P188:' + $RecoveryToken)) {
        throw 'P188 recovery journal invalid or missing.'
    }
    [IO.File]::WriteAllText($watchReady, 'P188_WATCHDOG_READY')
    $recoveryLock = $null
    try {
        while ($null -eq $recoveryLock) {
            try {
                $recoveryLock = [IO.File]::Open($watchLockPath,
                    [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite,
                    [IO.FileShare]::None)
            } catch {
                Start-Sleep -Milliseconds 50
            }
        }
        if (Test-Path -LiteralPath $watchJournal -PathType Leaf) {
            if (-not (Test-Path -LiteralPath $watchTarget) -and
                (Test-Path -LiteralPath $watchBackup -PathType Container)) {
                Move-Item -LiteralPath $watchBackup -Destination $watchTarget -ErrorAction Stop
                Write-Host 'P188_WATCHDOG_RESTORED_PREVIOUS_COMPLETE_BUNDLE'
            }
            if ((Test-Path -LiteralPath $watchTarget -PathType Container) -and
                (Test-Path -LiteralPath $watchStage -PathType Container)) {
                Remove-Item -LiteralPath $watchStage -Recurse -Force -ErrorAction Stop
            }
            if (Test-Path -LiteralPath $watchTarget -PathType Container) {
                Remove-Item -LiteralPath $watchJournal -Force -ErrorAction Stop
                if (Test-Path -LiteralPath $watchReady) {
                    Remove-Item -LiteralPath $watchReady -Force
                }
                if (Test-Path -LiteralPath $watchKillPoint) {
                    Remove-Item -LiteralPath $watchKillPoint -Force
                }
            }
        }
    } finally {
        if ($null -ne $recoveryLock) { $recoveryLock.Dispose() }
    }
    return
}
$sourceBundle = Join-Path $PSScriptRoot $bundleName
$manifestFile = Join-Path $PSScriptRoot 'INTEGRITAET-SHA256.json'
$moduleRelative = $bundleName + '/Contents/x86_64-win/AWE64_Gold_Native_P009.vst3'
$required = @(
    $moduleRelative,
    ($bundleName + '/Contents/Resources/moduleinfo.json'),
    ($bundleName + '/Contents/Resources/P009-Private/awe32.raw'),
    ($bundleName + '/Contents/Resources/P009-Private/SYNTHGM.SBK'),
    ($bundleName + '/Contents/Resources/P009-Private/user.sf2'),
    ($bundleName + '/Contents/Resources/P009-Private/77-presets.sf2'),
    ($bundleName + '/Contents/Resources/P009-Private/vintage.sf2')
)
if (-not (Test-Path -LiteralPath $sourceBundle -PathType Container) -or
    -not (Test-Path -LiteralPath $manifestFile -PathType Leaf)) {
    throw 'Privates VST3-Paket unvollstaendig (Bundle oder SHA256-Manifest fehlt).'
}
$manifest = Get-Content -LiteralPath $manifestFile -Encoding UTF8 -Raw | ConvertFrom-Json
if ($null -eq $manifest -or $null -eq $manifest.files -or
    @($manifest.files).Count -lt $required.Count -or
    [string]$manifest.vst3_module_sha256 -notmatch '^[0-9a-fA-F]{64}$') {
    throw 'SHA256-Manifest ungueltig oder nicht vollstaendig.'
}
$entries = @($manifest.files)
$seen = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
foreach ($entry in $entries) {
    $relative = [string]$entry.path
    if ($relative -notmatch '^[a-zA-Z0-9._/ -]+$' -or
        $relative -match '(^|/)[.][.]?(/|$)' -or
        [string]$entry.sha256 -notmatch '^[0-9a-fA-F]{64}$' -or
        -not $seen.Add($relative)) {
        throw ('Ungueltiger oder doppelter Manifestpfad: ' + $relative)
    }
}
foreach ($file in $required) {
    if (-not $seen.Contains($file)) {
        throw ('Pflichtdatei fehlt im Manifest: ' + $file)
    }
}
$moduleRecord = @($entries | Where-Object { [string]$_.path -eq $moduleRelative })
if ($moduleRecord.Count -ne 1 -or
    [string]$moduleRecord[0].sha256 -ne [string]$manifest.vst3_module_sha256) {
    throw 'Modul-SHA256 und Manifest stimmen nicht ueberein.'
}
function Assert-PackageFiles([string]$root, [bool]$bundleOnly) {
    foreach ($item in $entries) {
        $relative = [string]$item.path
        if ($bundleOnly -and -not $relative.StartsWith($bundleName + '/', [StringComparison]::OrdinalIgnoreCase)) {
            continue
        }
        $at = if ($bundleOnly) {
            $within = $relative.Substring($bundleName.Length + 1)
            Join-Path $root ($within.Replace('/', '\'))
        } else {
            Join-Path $root ($relative.Replace('/', '\'))
        }
        if (-not (Test-Path -LiteralPath $at -PathType Leaf)) {
            throw ('Paketdatei fehlt: ' + $relative)
        }
        $actual = Get-Item -LiteralPath $at
        if ([long]$actual.Length -ne [long]$item.bytes) {
            throw ('Dateigroesse falsch: ' + $relative)
        }
        $digest = (Get-FileHash -LiteralPath $at -Algorithm SHA256).Hash
        if (-not $digest.Equals([string]$item.sha256, [StringComparison]::OrdinalIgnoreCase)) {
            throw ('SHA256-Pruefung fehlgeschlagen: ' + $relative)
        }
    }
}
# These checks are intentionally completed before any target/backup mutation.
Assert-PackageFiles $PSScriptRoot $false
$found = @(Get-ChildItem -LiteralPath $PSScriptRoot -Recurse -File -Force |
           Where-Object { $_.FullName -ne $manifestFile })
if ($found.Count -ne $entries.Count) {
    throw 'Paket enthaelt unbekannte oder nicht aufgelistete Dateien.'
}
if ([string]::IsNullOrWhiteSpace($DestinationRoot)) {
    $principal = New-Object Security.Principal.WindowsPrincipal(
                      [Security.Principal.WindowsIdentity]::GetCurrent())
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        $arg = '-NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '"'
        $proc = Start-Process -FilePath 'powershell.exe' -Verb RunAs -ArgumentList $arg -Wait -PassThru
        if ($proc.ExitCode -ne 0) { throw ('UAC-Installation fehlgeschlagen: ' + $proc.ExitCode) }
        return
    }
    $common = [Environment]::GetFolderPath('CommonProgramFiles')
    if ([string]::IsNullOrWhiteSpace($common)) {
        throw 'Windows Common Files nicht gefunden.'
    }
    $parent = Join-Path $common 'VST3'
} else {
    # Explicit destination override is for isolated, disposable CI tests.
    # Normal owner installation always uses the Windows common VST3 folder.
    $parent = [IO.Path]::GetFullPath($DestinationRoot)
}
New-Item -ItemType Directory -Path $parent -Force | Out-Null
# A crash-safe per-destination interprocess mutex. FileShare.None must be
# acquired BEFORE observing or mutating target/backup. In particular a
# second installer that fails must never rollback an already committed first
# installer. Never delete the lock file: deleting allows inode replacement
# and a third installer to enter while an older handle is still held.
$installLockPath = Join-Path $parent ($bundleName + '.install.lock')
try {
    $installLock = [IO.File]::Open($installLockPath,
        [IO.FileMode]::OpenOrCreate,
        [IO.FileAccess]::ReadWrite,
        [IO.FileShare]::None)
} catch {
    throw ('Installation laeuft bereits; Ziel unveraendert: ' + $installLockPath)
}
try {
$target = Join-Path $parent $bundleName
# Recover abandoned P188 transactions BEFORE starting another install,
# while holding the same nonshareable cross-process lock. These are exact
# GUID journals; historical user backups with no matching journal are NEVER
# deleted or automatically overwritten.
$stale = @(Get-ChildItem -LiteralPath $parent -File -Filter ($bundleName + '.txn-*.journal'))
foreach ($entry in $stale) {
    $prefix = $bundleName + '.txn-'
    $suffix = '.journal'
    $name = $entry.Name
    if (-not $name.StartsWith($prefix, [StringComparison]::Ordinal) -or
        -not $name.EndsWith($suffix, [StringComparison]::Ordinal)) { continue }
    $token = $name.Substring($prefix.Length,
        $name.Length - $prefix.Length - $suffix.Length)
    if ($token -cnotmatch '^[0-9a-f]{32}$' -or
        [IO.File]::ReadAllText($entry.FullName) -cne ('P188:' + $token)) { continue }
    $oldBackup = Join-Path $parent ($bundleName + '.backup-' + $token)
    $oldStage = Join-Path $parent ($bundleName + '.stage-' + $token)
    if (-not (Test-Path -LiteralPath $target) -and
        (Test-Path -LiteralPath $oldBackup -PathType Container)) {
        Move-Item -LiteralPath $oldBackup -Destination $target -ErrorAction Stop
        Write-Host 'P188_ABANDONED_INSTALL_RESTORED_BY_NEXT_RUN'
    }
    if (Test-Path -LiteralPath $target -PathType Container) {
        if (Test-Path -LiteralPath $oldStage -PathType Container) {
            Remove-Item -LiteralPath $oldStage -Recurse -Force
        }
        Remove-Item -LiteralPath $entry.FullName -Force
        $oldReady = Join-Path $parent ($bundleName + '.txn-' + $token + '.ready')
        $oldKill = Join-Path $parent ($bundleName + '.killpoint-' + $token)
        if (Test-Path -LiteralPath $oldReady) {
            Remove-Item -LiteralPath $oldReady -Force
        }
        if (Test-Path -LiteralPath $oldKill) {
            Remove-Item -LiteralPath $oldKill -Force
        }
    }
}
$nonce = [Guid]::NewGuid().ToString('N')
$stage = Join-Path $parent ($bundleName + '.stage-' + $nonce)
$backup = Join-Path $parent ($bundleName + '.backup-' + $nonce)
$journal = Join-Path $parent ($bundleName + '.txn-' + $nonce + '.journal')
$ready = Join-Path $parent ($bundleName + '.txn-' + $nonce + '.ready')
$killpoint = Join-Path $parent ($bundleName + '.killpoint-' + $nonce)
$hadTarget = Test-Path -LiteralPath $target
$madeBackup = $false
$watchdog = $null
try {
    Copy-Item -LiteralPath $sourceBundle -Destination $stage -Recurse -Force -ErrorAction Stop
    # Verify the exact files *in the staged install*, not merely the ZIP.
    Assert-PackageFiles $stage $true
    if ($hadTarget) {
        # Never move the original without a separate recovery process that
        # already acknowledged the transaction. Crash-recovery journal and
        # stage both live on the target volume under the exclusive mutex.
        [IO.File]::WriteAllText($journal, 'P188:' + $nonce)
        $argsWatch = '-NoProfile -ExecutionPolicy Bypass -File "' +
            $PSCommandPath + '" -DestinationRoot "' + $parent +
            '" -RecoveryWatchdog -RecoveryToken ' + $nonce
        $watchdog = Start-Process -FilePath 'powershell.exe' -ArgumentList $argsWatch -WindowStyle Hidden -PassThru -ErrorAction Stop
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        while (-not (Test-Path -LiteralPath $ready -PathType Leaf)) {
            if ($watchdog.HasExited) {
                throw 'P188 recovery watchdog exited before readiness.'
            }
            if ([DateTime]::UtcNow -ge $deadline) {
                throw 'P188 recovery watchdog startup timeout.'
            }
            Start-Sleep -Milliseconds 50
        }
        if ([IO.File]::ReadAllText($ready) -cne 'P188_WATCHDOG_READY') {
            throw 'P188 recovery watchdog readiness corrupt.'
        }
        Move-Item -LiteralPath $target -Destination $backup -ErrorAction Stop
        $madeBackup = $true
        # Deterministic negative test hook ONLY in isolated CI destination.
        # Real owner installation never enables this test environment.
        if ($env:P188_INSTALLER_CI_KILLPOINT -eq '1' -and $NoElevation -and
            -not [string]::IsNullOrWhiteSpace($DestinationRoot)) {
            [IO.File]::WriteAllText($killpoint, 'P188_AFTER_OLD_TARGET_RENAMED')
            Start-Sleep -Seconds 60
        }
    }
    # A same-volume directory rename publishes a complete stage, not a
    # partially copied plugin. The canonical name is briefly absent before
    # this second rename. After forced process termination the watchdog
    # restores the backup, without ever touching existing older backups.
    Move-Item -LiteralPath $stage -Destination $target -ErrorAction Stop
    Assert-PackageFiles $target $true
    Write-Host ('125A AWE64 Gold installiert: ' + $target)
    if ($madeBackup) { Write-Host ('Vorherige Version gesichert: ' + $backup) }
    Write-Host 'VST3-Paket vollstaendig per SHA256 geprueft.'
} catch {
    # Failure is never allowed to strand a displaced previous plugin.
    if ($madeBackup) {
        if (Test-Path -LiteralPath $target) {
            Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction Stop
        }
        Move-Item -LiteralPath $backup -Destination $target -ErrorAction Stop
    } elseif (-not $hadTarget -and (Test-Path -LiteralPath $target)) {
        Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction Stop
    }
    throw
} finally {
    if (Test-Path -LiteralPath $stage) {
        Remove-Item -LiteralPath $stage -Recurse -Force
    }
}
} finally {
    # Releasing in an outer finally also covers failures that occur before
    # the staging/catch block (e.g. target metadata access).
    $installLock.Dispose()
    # On a normal return, allow the watchdog to observe the released lock
    # and remove its journal before we report a clean install. The forcibly
    # killed installer never runs this finally; its independent watchdog
    # still holds the repair responsibility.
    if ($null -ne $watchdog) {
        if (-not $watchdog.WaitForExit(15000)) {
            Write-Warning 'P188 recovery watchdog still running; transaction journal retained.'
        } elseif ($watchdog.ExitCode -ne 0) {
            Write-Warning ('P188 recovery watchdog exited with: ' + $watchdog.ExitCode)
        }
        $watchdog.Dispose()
    }
}
