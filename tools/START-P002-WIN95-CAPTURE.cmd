@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "ROOT=%~dp0"
set "ARCHIVE=%ROOT%125A-AWE64-P002-Win95-CC0-korrigiert-PRIVATE.zip"
if not exist "%ARCHIVE%" (
  echo Originales privates Win95-Paket fehlt:
  echo "%ARCHIVE%"
  echo Dieses Paket NICHT nach GitHub hochladen.
  pause
  exit /b 2
)
where py >nul 2>nul
if errorlevel 1 (
  echo Python 3 unter Windows nicht gefunden. py -3 ist erforderlich.
  pause
  exit /b 3
)
for /f %%T in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "STAMP=%%T"
if not defined STAMP exit /b 4
set "WORK=%ROOT%PRIVATE-P002-WIN95-%STAMP%-%RANDOM%"
if exist "%WORK%" exit /b 5
echo.
echo 125A P002: Echtes ORIGINAL-Creative-Win95 im kalibrierten 86Box-Fork.
echo Die VM oeffnet sich jetzt interaktiv. Nach dem Test 86Box beenden.
echo Register und Audio werden als neue Dateien der EINEN Sitzung gesichert.
echo Alle Creative-, Windows- und Sample-Daten bleiben PRIVAT auf dem PC.
echo.
py -3 "%ROOT%p002_owner_launch_original_vm.py" --private-original-archive "%ARCHIVE%" --new-private-workdir "%WORK%"
if errorlevel 1 (
  echo.
  echo Kein Originaltreiber-/DRY-Abnahmenachweis. Details siehe Meldung oben.
  echo Private Untersuchungsdateien: "%WORK%"
  pause
  exit /b 1
)
echo.
echo Original-Win95-Gastlauf, RAW-Register, DRY-PCM und SHA-Pruefung bestanden.
echo Private Aufzeichnungen: "%WORK%\P002-Nachweise"
echo NICHT nach GitHub hochladen; VST3-Paritaet ist gesondert zu messen.
pause
exit /b 0
