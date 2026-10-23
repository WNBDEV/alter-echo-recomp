@echo off
rem Starts Alter Echo on Windows. Settings: settings.ini (made on first
rem run); log: logs\run-<date>-<time>.txt. Double-clicking
rem build-win\alter_echo_recomp.exe works too.
cd /d "%~dp0"
start "" "build-win\alter_echo_recomp.exe" %*
