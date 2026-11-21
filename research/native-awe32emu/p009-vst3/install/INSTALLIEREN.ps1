# 125A AWE64 Gold – complete, private Windows-x64-VST3 installer.
# When -TargetRoot is omitted this performs the standard, elevated system
# install into Common Files\VST3. Custom -TargetRoot is for a user-owned
# destination and enables an isolated Windows CI smoke test without UAC.
# NEVER download or disclose private Creative ROM/SBK/SF2 assets.
param(
    [string]$TargetRoot = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$source = Join-Path $PSScriptRoot 'AWE64_Gold_Native_P009.vst3'
$required = @(
    'Contents\x86_64-win\AWE64_Gold_Native_P009.vst3',
    'Contents\Resources\moduleinfo.json',
    'Contents\Resources\P009-Private\awe32.raw',
    'Contents\Resources\P009-Private\SYNTHGM.SBK',
    'Contents\Resources\P009-Private\user.sf2'
)
if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw 'Complete VST3 bundle directory is missing.'
}
foreach ($relative in $required) {
    $file = Join-Path $source $relative
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Incomplete VST3 bundle: missing $relative"
    }
}
$defaultRoot = [Environment]::GetFolderPath('CommonProgramFiles')
if ([string]::IsNullOrWhiteSpace($defaultRoot)) {
    throw 'Windows Common Program Files directory unavailable.'
}
$systemWide = [string]::IsNullOrWhiteSpace($TargetRoot)
if ($systemWide) {
    $TargetRoot = $defaultRoot
    $principal = New-Object Security.Principal.WindowsPrincipal(
        [Security.Principal.WindowsIdentity]::GetCurrent())
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        $args = '-NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '"'
        $raised = Start-Process -FilePath 'powershell.exe' -ArgumentList $args -Verb RunAs -Wait -PassThru
        if ($raised.ExitCode -ne 0) {
            throw "Elevated AWE64 Gold installation failed ($($raised.ExitCode))"
        }
        exit 0
    }
}
$TargetRoot = [IO.Path]::GetFullPath($TargetRoot)
$parent = Join-Path $TargetRoot 'VST3'
$target = Join-Path $parent 'AWE64_Gold_Native_P009.vst3'
New-Item -ItemType Directory -Path $parent -Force | Out-Null

$backup = $null
if (Test-Path -LiteralPath $target) {
    $suffix = Get-Date -Format 'yyyyMMdd-HHmmss-ffff'
    $backup = Join-Path $parent ('AWE64_Gold_Native_P009.backup-' + $suffix)
    if (Test-Path -LiteralPath $backup) {
        throw 'An existing backup has the same name. Installation stopped.'
    }
    Move-Item -LiteralPath $target -Destination $backup -ErrorAction Stop
    Write-Host "Previous complete VST3 preserved at $backup"
}
try {
    # Install the VST3 as an indivisible folder. Never intermix old ROM,
    # SF2 or GUI resources with a newly built x64 module.
    Copy-Item -LiteralPath $source -Destination $target -Recurse -Force -ErrorAction Stop
    $sourceFiles = @(Get-ChildItem -LiteralPath $source -Recurse -File -ErrorAction Stop)
    if ($sourceFiles.Count -lt 8) {
        throw 'Source VST3 contains unexpectedly few files.'
    }
    foreach ($item in $sourceFiles) {
        $relative = $item.FullName.Substring($source.Length).TrimStart('\','/')
        $installed = Join-Path $target $relative
        if (-not (Test-Path -LiteralPath $installed -PathType Leaf)) {
            throw "Installation lost VST3 resource: $relative"
        }
        $expected = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
        $actual = (Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash
        if ($expected -ne $actual) {
            throw "Installation changed VST3 resource bytes: $relative"
        }
    }
    Write-Host "125A AWE64 Gold VST3 installed and verified: $target"
    Write-Host 'Restart Fender Studio Pro and rescan VST3 instruments if necessary.'
} catch {
    # Restore the previous complete plugin, not a partially copied hybrid.
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
    if ($null -ne $backup -and (Test-Path -LiteralPath $backup)) {
        Move-Item -LiteralPath $backup -Destination $target -ErrorAction Stop
    }
    throw
}
