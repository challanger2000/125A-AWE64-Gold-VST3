@echo off
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0"
title 125A P003 - Original Creative Win95 Zweibank-Messung
echo.
echo ========================================================
echo    125A P003 - Original Creative Windows 95 Zweibanktest
echo ========================================================
echo.
echo Die private Original-VM wird NUR KOPIERT, niemals veraendert.
echo Der zweite Samplebank-Test und echte EMU8000-Spuren werden
echo nach dem Beenden von 86Box in einer Ergebnis-ZIP gesammelt.
echo.
if not exist "%~dp0125A-P003-TEST.exe" (
  echo FEHLER: 125A-P003-TEST.exe fehlt im Paket.
  echo Bitte die gesamte Original-ZIP entpacken.
  pause
  exit /b 2
)
"%~dp0125A-P003-TEST.exe" %*
set "CODE=%ERRORLEVEL%"
echo.
if "%CODE%"=="0" (
  echo P003-Originalmessung aufgezeichnet.
  echo Lade die neu erzeugte P003-*-ERGEBNIS.zip im Chat hoch.
) else (
  echo P003 NO-GO: Die Quelle konnte nicht vollstaendig gemessen werden.
  echo Bitte lade die P003-*-ERGEBNIS.zip hoch, falls vorhanden.
  echo Die originale private VM wurde nicht veraendert.
)
echo.
pause
exit /b %CODE%
