@echo off
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0"
title 125A P120 - Original Creative CPU-Zeitmessung
echo.
echo ==========================================================
echo   P120 ORIGINAL WIN95 - P54C CPU / JIT / EMU8000 MESSUNG
echo ==========================================================
echo.
echo Das vorhandene private Win95-/86Box-Paket wird geprueft und
echo NUR IN EINER KOPIE gestartet. Die originale ZIP bleibt intakt.
echo.
if not exist "%~dp0125A-P120-CAPTURE.exe" (
  echo FEHLER: P120-Starter fehlt. Gesamtpaket erneut entpacken.
  pause
  exit /b 2
)
if not exist "%~dp086Box.exe" (
  echo FEHLER: Originaler instrumentierter 86Box-Fork fehlt.
  pause
  exit /b 2
)
if not exist "%~dp0P002RAM.EXE" (
  echo FEHLER: Originaltest-Gastprogramm P002RAM.EXE fehlt.
  pause
  exit /b 2
)
"%~dp0125A-P120-CAPTURE.exe" %*
set "CODE=%ERRORLEVEL%"
echo.
if "%CODE%"=="0" (
  echo P120: Originaltreiber-Messung nach allen Audits PASS.
  echo Die entstandene P120-Original-CPU-Messung-...ERGEBNIS.zip
  echo bitte NUR privat in den Entwicklerchat oder Kontrollchat geben.
) else (
  echo P120 NO-GO. Die originale private VM ist unveraendert.
  echo Die P120-Original-CPU-Messung-...ERGEBNIS.zip enthaelt
  echo den Grund bzw. vorhandene Messspuren fuer die Diagnose.
)
echo.
pause
exit /b %CODE%
