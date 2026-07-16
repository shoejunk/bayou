@echo off
if not defined BAYOU_DEV_TLS_READY (
    call "%~dp0ensure_dev_tls.bat" || exit /b 1
)
"%~dp0build\Debug\matchmaking.exe"
