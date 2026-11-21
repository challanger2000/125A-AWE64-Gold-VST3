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
# Journal paths are derived from the nonce, never from serialized path data.
function Get-TransactionPaths([string]$root, [string]$token) {
    if ($token -cnotmatch '^[0-9a-f]{32}$') { throw 'P189 invalid recovery token.' }
    $prefix = Join-Path $root $bundleName
    @{
        Target = $prefix
        Stage = $prefix + '.stage-' + $token
        Backup = $prefix + '.backup-' + $token
        Journal = $prefix + '.txn-' + $token + '.journal'
        Failure = $prefix + '.txn-' + $token + '.failure'
        Ready = $prefix + '.txn-' + $token + '.ready'
        KillPoint = $prefix + '.killpoint-' + $token
    }
}

function Get-TextSha256([string]$data) {
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes($data)
        [BitConverter]::ToString($algorithm.ComputeHash($bytes)).Replace('-', '').ToLowerInvariant()
    } finally { $algorithm.Dispose() }
}

# Include directory names (even empty directories), every relative filename,
# file length and file SHA. Reject junctions/symlinks rather than traversing
# outside the staged or backed-up bundle. Ordering is independent of locale.
function Get-BundleTreeSha256([string]$root) {
    $base = Get-Item -LiteralPath $root -Force -ErrorAction Stop
    if (-not $base.PSIsContainer -or
        ($base.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw ('P189 invalid bundle directory: ' + $root)
    }
    $records = @(
        foreach ($entry in Get-ChildItem -LiteralPath $base.FullName -Recurse -Force -ErrorAction Stop) {
            if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw ('P189 linked bundle entry rejected: ' + $entry.FullName)
            }
            $relative = $entry.FullName.Substring($base.FullName.Length + 1).Replace('\', '/')
            $name = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($relative))
            if ($entry.PSIsContainer) {
                'D:' + $name
            } else {
                $sha = (Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256 -ErrorAction Stop).Hash
                'F:' + $name + ':' + $entry.Length + ':' + $sha
            }
        }
    )
    [Array]::Sort($records, [StringComparer]::Ordinal)
    Get-TextSha256 ("P189-BUNDLE`n" + [string]::Join("`n", $records))
}

function Write-InstallJournal($paths, [string]$token, [string]$before, [string]$after) {
    $body = "P189:${token}:${before}:${after}"
    $bytes = [Text.Encoding]::UTF8.GetBytes($body + ':' + (Get-TextSha256 $body))
    $temporary = $paths.Journal + '.new'
    $file = [IO.File]::Open($temporary, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $file.Write($bytes, 0, $bytes.Length)
        $file.Flush($true)
    } finally { $file.Dispose() }
    [IO.File]::Move($temporary, $paths.Journal)
}

function Read-InstallJournal($paths, [string]$token) {
    $record = [IO.File]::ReadAllText($paths.Journal)
    if ($record -cnotmatch '\A(P189:([0-9a-f]{32}):(-|[0-9a-f]{64}):([0-9a-f]{64})):([0-9a-f]{64})\z' -or
        $Matches[2] -cne $token -or (Get-TextSha256 $Matches[1]) -cne $Matches[5]) {
        throw 'P189 recovery journal has no valid integrity record.'
    }
    @{ Before = $Matches[3]; After = $Matches[4] }
}

function Assert-BackupTree($paths, [string]$expected) {
    if (-not (Test-Path -LiteralPath $paths.Backup -PathType Container) -or
        (Get-BundleTreeSha256 $paths.Backup) -cne $expected) {
        throw 'P189 recovery backup is missing or its tree SHA256 does not match.'
    }
}

# Both the watchdog and a later installer use this same validation while
# holding the destination lock. Unknown targets and damaged backups preserve
# the journal, stage and backup for diagnosis instead of completing recovery.
function Complete-InstallTransaction($paths, [string]$token, [switch]$Rollback) {
    try {
        $journal = Read-InstallJournal $paths $token
        if (Test-Path -LiteralPath $paths.Target) {
            $actual = Get-BundleTreeSha256 $paths.Target
            if ($actual -cne $journal.Before -and $actual -cne $journal.After) {
                throw 'P189 recovery target tree does not match either recorded bundle.'
            }
            # A synchronous install failure may roll back a verified stage
            # that was published before the error. Preserve it until the old
            # tree is validated and restored. An unknown tree stays in place.
            if ($Rollback -and $actual -cne $journal.Before) {
                if ($journal.Before -cne '-') { Assert-BackupTree $paths $journal.Before }
                if (Test-Path -LiteralPath $paths.Stage) {
                    throw 'P189 rollback staging path is occupied.'
                }
                Move-Item -LiteralPath $paths.Target -Destination $paths.Stage -ErrorAction Stop
            }
        }
        if (-not (Test-Path -LiteralPath $paths.Target) -and $journal.Before -cne '-') {
            Assert-BackupTree $paths $journal.Before
            Move-Item -LiteralPath $paths.Backup -Destination $paths.Target -ErrorAction Stop
            if ((Get-BundleTreeSha256 $paths.Target) -cne $journal.Before) {
                throw 'P189 restored tree failed post-rename verification.'
            }
            Write-Host 'P188_ABANDONED_INSTALL_RESTORED_BY_NEXT_RUN'
        }
        if (Test-Path -LiteralPath $paths.Stage) {
            if ((Get-BundleTreeSha256 $paths.Stage) -cne $journal.After) {
                throw 'P189 recovery retained a changed staging tree.'
            }
            Remove-Item -LiteralPath $paths.Stage -Recurse -Force -ErrorAction Stop
        }
        foreach ($marker in @($paths.Ready, $paths.KillPoint, $paths.Failure)) {
            if (Test-Path -LiteralPath $marker) { Remove-Item -LiteralPath $marker -Force -ErrorAction Stop }
        }
        Remove-Item -LiteralPath $paths.Journal -Force -ErrorAction Stop
    } catch {
        [IO.File]::WriteAllText($paths.Failure, $_.Exception.Message)
        throw
    }
}

# The watchdog uses the same per-destination lock after the owner's process
# has exited. It recovers a verified tree; directory exchange has a gap.
if ($RecoveryWatchdog) {
    if ([string]::IsNullOrWhiteSpace($DestinationRoot)) {
        throw 'P189 recovery watchdog requires a destination.'
    }
    $watchRoot = [IO.Path]::GetFullPath($DestinationRoot)
    $paths = Get-TransactionPaths $watchRoot $RecoveryToken
    $null = Read-InstallJournal $paths $RecoveryToken
    [IO.File]::WriteAllText($paths.Ready, 'P188_WATCHDOG_READY')
    $watchLockPath = Join-Path $watchRoot ($bundleName + '.install.lock')
    $recoveryLock = $null
    $deadline = [DateTime]::UtcNow.AddMinutes(2)
    try {
        while ($null -eq $recoveryLock) {
            try {
                $recoveryLock = [IO.File]::Open($watchLockPath,
                    [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite,
                    [IO.FileShare]::None)
            } catch {
                if ([DateTime]::UtcNow -ge $deadline) {
                    throw 'P189 watchdog could not take the lock. Recovery journal retained.'
                }
                Start-Sleep -Milliseconds 50
            }
        }
        if (Test-Path -LiteralPath $paths.Journal -PathType Leaf) {
            Complete-InstallTransaction $paths $RecoveryToken
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
$watchdog = $null
try {
    # Reconcile only known, validated transactions. A legacy P188 journal
    # lacks tree evidence; retain it and its data for explicit recovery.
    $prefix = $bundleName + '.txn-'
    $suffix = '.journal'
    foreach ($entry in Get-ChildItem -LiteralPath $parent -File -Filter ($prefix + '*' + $suffix)) {
        $name = $entry.Name
        $token = $name.Substring($prefix.Length, $name.Length - $prefix.Length - $suffix.Length)
        $abandoned = Get-TransactionPaths $parent $token
        Complete-InstallTransaction $abandoned $token
    }

    $nonce = [Guid]::NewGuid().ToString('N')
    $paths = Get-TransactionPaths $parent $nonce
    $before = '-'
    if (Test-Path -LiteralPath $paths.Target) {
        $before = Get-BundleTreeSha256 $paths.Target
    }
    $after = Get-BundleTreeSha256 $sourceBundle
    try {
        Copy-Item -LiteralPath $sourceBundle -Destination $paths.Stage -Recurse -Force -ErrorAction Stop
        Assert-PackageFiles $paths.Stage $true
        if ((Get-BundleTreeSha256 $paths.Stage) -cne $after) {
            throw 'P189 staged tree does not match the verified package.'
        }
        Write-InstallJournal $paths $nonce $before $after
        $argsWatch = '-NoProfile -ExecutionPolicy Bypass -File "' +
            $PSCommandPath + '" -DestinationRoot "' + $parent +
            '" -RecoveryWatchdog -RecoveryToken ' + $nonce
        $watchdog = Start-Process -FilePath 'powershell.exe' -ArgumentList $argsWatch -WindowStyle Hidden -PassThru -ErrorAction Stop
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        while (-not (Test-Path -LiteralPath $paths.Ready -PathType Leaf)) {
            if ($watchdog.HasExited) { throw 'P188 recovery watchdog exited before readiness.' }
            if ([DateTime]::UtcNow -ge $deadline) { throw 'P188 recovery watchdog startup timeout.' }
            Start-Sleep -Milliseconds 50
        }
        if ($watchdog.HasExited -or [IO.File]::ReadAllText($paths.Ready) -cne 'P188_WATCHDOG_READY') {
            throw 'P188 recovery watchdog readiness corrupt.'
        }
        if ($before -cne '-') {
            if ((Get-BundleTreeSha256 $paths.Target) -cne $before) {
                throw 'P189 previous bundle changed before backup.'
            }
            Move-Item -LiteralPath $paths.Target -Destination $paths.Backup -ErrorAction Stop
            Assert-BackupTree $paths $before
            # Retained for the native CI forced-process-kill regression.
            if ($env:P188_INSTALLER_CI_KILLPOINT -eq '1' -and $NoElevation -and
                -not [string]::IsNullOrWhiteSpace($DestinationRoot)) {
                [IO.File]::WriteAllText($paths.KillPoint, 'P188_AFTER_OLD_TARGET_RENAMED')
                Start-Sleep -Seconds 60
            }
        }
        if (Test-Path -LiteralPath $paths.Target) { throw 'P189 publication target is occupied.' }
        Move-Item -LiteralPath $paths.Stage -Destination $paths.Target -ErrorAction Stop
        if ((Get-BundleTreeSha256 $paths.Target) -cne $after) {
            throw 'P189 published tree failed verification.'
        }
        Complete-InstallTransaction $paths $nonce
        Write-Host ('125A AWE64 Gold installiert: ' + $paths.Target)
        if ($before -cne '-') { Write-Host ('Vorherige Version gesichert: ' + $paths.Backup) }
        Write-Host 'VST3-Paket vollstaendig per SHA256 geprueft.'
    } catch {
        if (Test-Path -LiteralPath $paths.Journal -PathType Leaf) {
            Complete-InstallTransaction $paths $nonce -Rollback
        } elseif (Test-Path -LiteralPath $paths.Stage) {
            Remove-Item -LiteralPath $paths.Stage -Recurse -Force -ErrorAction Stop
        }
        throw
    }
} finally {
    $installLock.Dispose()
    if ($null -ne $watchdog) {
        if (-not $watchdog.WaitForExit(15000)) {
            Write-Warning 'P188 recovery watchdog still running; transaction journal retained.'
        } elseif ($watchdog.ExitCode -ne 0) {
            Write-Warning ('P188 recovery watchdog exited with: ' + $watchdog.ExitCode)
        }
        $watchdog.Dispose()
    }
}
