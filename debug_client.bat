@echo off
call "%~dp0ensure_dev_tls.bat" || exit /b 1
set "BAYOU_TLS_CA_FILE=%~dp0tls\dev-client-ca-bundle.pem"
"%~dp0build\Debug\SteamTactics.exe"
