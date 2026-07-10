@echo off
call "%~dp0ensure_dev_tls.bat" || exit /b 1
"%~dp0build\Debug\gametest.exe"
