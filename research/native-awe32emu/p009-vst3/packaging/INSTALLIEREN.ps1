# 125A AWE64 Gold — private Windows VST3 installation
# The complete bundle and its full SHA256 manifest are verified BEFORE
# replacing an existing instrument. Never install a partial directory.
[CmdletBinding()]
param(
    [string]$DestinationRoot = '',
    [switch]$NoElevation
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$bundleName = 'AWE64_Gold_Native_P009.vst3'
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
$target = Join-Path $parent $bundleName
$nonce = [Guid]::NewGuid().ToString('N')
$stage = Join-Path $parent ($bundleName + '.stage-' + $nonce)
$backup = Join-Path $parent ($bundleName + '.backup-' +
                              (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' +
                              $nonce.Substring(0, 8))
$hadTarget = Test-Path -LiteralPath $target
$madeBackup = $false
try {
    Copy-Item -LiteralPath $sourceBundle -Destination $stage -Recurse -Force -ErrorAction Stop
    # Verify the exact files *in the staged install*, not merely the ZIP.
    Assert-PackageFiles $stage $true
    if ($hadTarget) {
        Move-Item -LiteralPath $target -Destination $backup -ErrorAction Stop
        $madeBackup = $true
    }
    # Directory rename within one volume: no DAW ever sees a half-copy.
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
