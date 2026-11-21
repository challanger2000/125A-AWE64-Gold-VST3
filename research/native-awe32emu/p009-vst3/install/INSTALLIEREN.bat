@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo 125A AWE64 Gold - private complete Windows VST3 installer
echo Close Fender Studio Pro and all other DAWs first.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0INSTALLIEREN.ps1"
if errorlevel 1 (echo Installation FAILED.) else (echo Installation verified.)
pause
