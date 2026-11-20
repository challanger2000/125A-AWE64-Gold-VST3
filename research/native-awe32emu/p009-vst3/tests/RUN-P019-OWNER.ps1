# 125A AWE64 Gold – P019 PRIVATE owner PCM latency feasibility test.
# Windows PowerShell 5.1, no Python, no Studio Pro, NO GitHub uploads.
# Original ROM + SYNTHGM + private SF2 stay inside the user's unpacked ZIP.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$private = Join-Path $root "PRIVATE"
$tools = Join-Path $root "TOOLS"
$out = Join-Path $root "P019-ERGEBNISSE"
New-Item -ItemType Directory -Path $out -Force | Out-Null
$report = Join-Path $out "P019-ERGEBNIS.txt"
Set-Content -LiteralPath $report -Value "P019_OWNER_86BOX_EMU8000_BLOCK_WINDOW_COMPARE_20261120" -Encoding UTF8
function Write-Result([string]$line) {
    Write-Host $line
    Add-Content -LiteralPath $report -Value $line -Encoding UTF8
}
function Require-File([string]$file) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Benötigte Datei fehlt: $file"
    }
}
try {
    $rom = Join-Path $private "awe32.raw"
    $gm = Join-Path $private "SYNTHGM.SBK"
    $sf2 = Join-Path $private "77-presets.sf2"
    $vintage = Join-Path $private "vintage.sf2"
    foreach ($path in @($rom,$gm,$sf2,$vintage)) { Require-File $path }
    if ((Get-Item -LiteralPath $rom).Length -ne 1048576) {
        throw "Die Original-ROM hat keine 1 MiB"
    }
    $comparator = Join-Path $tools "P019ComparePCM.exe"
    Require-File $comparator
    $cases = @(
        @{ Name = "ROM-Program7"; Bank = 0; Program = 7; SF2 = $sf2 },
        @{ Name = "User-Program7"; Bank = 1; Program = 7; SF2 = $sf2 },
        @{ Name = "User-Program33"; Bank = 1; Program = 33; SF2 = $sf2 },
        @{ Name = "User-Program34"; Bank = 1; Program = 34; SF2 = $sf2 },
        @{ Name = "User-Program54"; Bank = 1; Program = 54; SF2 = $sf2 },
        @{ Name = "User-Program77"; Bank = 1; Program = 77; SF2 = $sf2 },
        @{ Name = "Vintage-Program7"; Bank = 1; Program = 7; SF2 = $vintage }
    )
    $failed = 0
    $passed = 0
    Write-Result "P019_PRIVATE_ASSETS_LOCAL_ONLY cases=7 modes=DRY,FX windows=980,128,64"
    Write-Result "P019_NO_STUDIO_PRO_NO_WIN95_VM_NO_DSP_CHANGES"
    foreach ($test in $cases) {
        foreach ($mode in @("dry","fx")) {
            $refFile = Join-Path $out ("{0}-{1}-980.pcm" -f $test.Name,$mode)
            $captureOkay = $true
            foreach ($window in @(980,128,64)) {
                $exe = Join-Path $tools ("P019Probe{0}.exe" -f $window)
                Require-File $exe
                $pcm = Join-Path $out ("{0}-{1}-{2}.pcm" -f $test.Name,$mode,$window)
                Write-Result ("P019_OWNER_CAPTURING {0} {1} window={2}" -f $test.Name,$mode,$window)
                $lines = & $exe $rom $gm $test.SF2 $mode ([string]$test.Bank) ([string]$test.Program) $pcm 2>&1
                foreach ($line in $lines) { Write-Result ([string]$line) }
                if ($LASTEXITCODE -ne 0) {
                    Write-Result ("P019_OWNER_CAPTURE_FAIL case={0} mode={1} window={2} code={3}" -f $test.Name,$mode,$window,$LASTEXITCODE)
                    $captureOkay = $false
                    $failed += 2
                    break
                }
            }
            if (-not $captureOkay) { continue }
            foreach ($window in @(128,64)) {
                $candidate = Join-Path $out ("{0}-{1}-{2}.pcm" -f $test.Name,$mode,$window)
                $label = "{0}_{1}" -f $test.Name,$mode
                $lines = & $comparator $refFile $candidate ([string]$window) $label 2>&1
                foreach ($line in $lines) { Write-Result ([string]$line) }
                if ($LASTEXITCODE -eq 0) { $passed++ }
                else { $failed++ }
            }
        }
    }
    Write-Result ("P019_PRIVATE_ORIGINAL_ROM_SBK_SF2_LATENCY_COMPARE_SUMMARY pass={0} fail={1} expected=28" -f $passed,$failed)
    if ($failed -ne 0 -or $passed -ne 28) {
        Write-Result "P019_PRIVATE_64_FRAME_PRODUCTION_NO_GO"
        Write-Result "P019_TEST_EXITCODE=1"
        exit 1
    }
    Write-Result "P019_PRIVATE_ORIGINAL_ROM_SBK_SF2_CHIP_980_128_64_PCM_BIT_EXACT_PASS_28_OF_28"
    Write-Result "P019_PRIVATE_CHIP_PARITY_ONLY_NOT_WIN95_HARDWARE_REFERENCE"
    Write-Result "P019_TEST_EXITCODE=0"
    exit 0
} catch {
    Write-Result ("P019_PRIVATE_TEST_ERROR " + $_.Exception.Message)
    Write-Result "P019_TEST_EXITCODE=2"
    exit 2
}
