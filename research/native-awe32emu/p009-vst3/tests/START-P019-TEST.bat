@echo off
setlocal
title 125A AWE64 Gold - P019 Originalklang Latenztest
cd /d "%~dp0"
echo ==============================================================
echo 125A AWE64 Gold - P019 Privater Originalklang-Latenzvergleich
echo ==============================================================
echo.
echo Vergleich des originalen EMU8000-Chipkerns bei 980/128/64 Samples.
echo Original-ROM, Creative-SBK und beide SF2 bleiben nur auf deinem PC.
echo Studio Pro wird NICHT gestartet. P007 bleibt unveraendert.
echo.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0RUN-P019-OWNER.ps1"
set "RC=%ERRORLEVEL%"
echo.
echo TEST BEENDET. Exitcode: %RC%
echo Ergebnispfad: P019-ERGEBNISSE\P019-ERGEBNIS.txt
echo Lade bitte nur diese Ergebnisdatei hoch.
echo.
pause
exit /b %RC%
