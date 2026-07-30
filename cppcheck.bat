@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\run_cppcheck.ps1"
exit /b %ERRORLEVEL%
