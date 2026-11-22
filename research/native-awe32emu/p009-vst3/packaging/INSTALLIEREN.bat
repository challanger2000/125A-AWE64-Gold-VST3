@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"
echo 125A AWE64 Gold - privater VST3-Installer
echo Bitte vorher Fender Studio Pro und andere DAWs schliessen.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0INSTALLIEREN.ps1"
if errorlevel 1 (
  echo Installation fehlgeschlagen. Die vorige Version bleibt erhalten.
) else (
  echo Installation und SHA256-Integritaetspruefung abgeschlossen.
)
pause
