@echo off
setlocal
rem 125A P001: real Creative ROM direct-core PCM only, no 86Box VM claim.
rem This script does not upload or commit ROM, SBK or recordings.
if "%~2"=="" (
 echo Usage: P001-PRIVATE-REAL-ROM.bat "C:\PRIVATE\awe32.raw" "C:\PRIVATE\SYNTHGM.SBK"
 exit /b 2
)
set "ROOT=%~dp0.."
set "OUT=%TEMP%\125A-P001-Private-Original-ROM-%RANDOM%-%RANDOM%"
cmake -S "%ROOT%" -B "%ROOT%\build" -A x64 || exit /b 3
cmake --build "%ROOT%\build" --config Release --parallel 2 || exit /b 4
python "%ROOT%\tools\p001_real_rom_check.py" --rom "%~1" --sbk "%~2" --direct "%ROOT%\build\Release\awe64_direct_fork_fixture.exe" --replay "%ROOT%\build\Release\awe64_replay.exe" --out "%OUT%"
if errorlevel 1 (
 echo P001 Real-ROM FAIL. Private diagnostics: %OUT%
 exit /b 5
)
echo P001 Real-ROM PCM/reference WAVs and numeric report: %OUT%
echo Independent running-86Box original Creative PCM parity: NOT RUN
