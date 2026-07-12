@echo off
call "%~dp0ensure_dev_tls.bat" || exit /b 1
set "BAYOU_TLS_CA_FILE=%~dp0tls\dev-client-ca-bundle.pem"
set "SERVER_CONFIG=%~dp0gameserver.cfg"
if not exist "%SERVER_CONFIG%" set "SERVER_CONFIG=%~dp0gameserver.cfg.example"
"%~dp0build\Debug\accounts.exe" --config "%SERVER_CONFIG%"
