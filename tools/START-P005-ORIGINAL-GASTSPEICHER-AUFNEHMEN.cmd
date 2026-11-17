@echo off
setlocal EnableExtensions
REM 125A P005: READ ONLY original Creative Win95 VXD bank capture.
REM Never write to 86Box memory, original disk, driver or SoundFont.
REM Start the original Win95 86Box VM and allow its source bank to load
REM before running this private owner-side capture launcher.

set "P005_CAPTURE=%~dp0..\build-p002\Release\awe64_p005_original_win95_guest_bank_capture.exe"
if not exist "%P005_CAPTURE%" set "P005_CAPTURE=%~dp0awe64_p005_original_win95_guest_bank_capture.exe"
if not exist "%P005_CAPTURE%" (
  echo.
  echo P005 NO-GO: Original-Win95-Capture-Werkzeug nicht gefunden.
  echo Bitte das komplette Windows-CI-Paket entpacken.
  pause
  exit /b 2
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop';" ^
  "$p=@(Get-Process -Name '86Box' -ErrorAction SilentlyContinue);" ^
  "if($p.Count -ne 1){throw ('Genau eine laufende 86Box-VM erforderlich. Gefunden: '+$p.Count)};" ^
  "$dir=Join-Path $env:LOCALAPPDATA '125A\PrivateOriginalWin95Captures';" ^
  "New-Item -ItemType Directory -Force -Path $dir | Out-Null;" ^
  "$tag=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8);" ^
  "$output=Join-Path $dir ('P005-Original-VXD-'+$tag+'.125a-private-vxd');" ^
  "$nested=Join-Path $dir ('P005-Original-Nested-'+$tag+'.125a-private-regions');" ^
  "Write-Host ('P005: nur lesender 86Box-RAM-Zugriff; Prozess-ID '+$p[0].Id);" ^
  "& '%P005_CAPTURE%' --pid $p[0].Id --output $output --nested-output $nested;" ^
  "if($LASTEXITCODE -ne 0){throw 'P005 NO-GO: Originalbank nicht eindeutig/stabil aufnehmbar'};" ^
  "Write-Host ('P005 PRIVATES Gast-Speicherpaket: '+$output);" ^
  "Write-Host ('P005 PRIVATE verschachtelte Quellbereiche: '+$nested);" ^
  "Write-Host 'HINWEIS: Treiber- und Sitzungsprovenienz noch getrennt pruefen. KEIN VST3-GO.'"
if errorlevel 1 (
  echo.
  echo Aufnahme nicht erfolgreich. Originaldateien wurden nicht veraendert.
  pause
  exit /b 1
)
echo.
echo Original-Gast-Speicheraufnahme angelegt. Bitte nur PRIVAT behandeln.
pause
exit /b 0
