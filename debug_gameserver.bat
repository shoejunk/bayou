@echo off
call "%~dp0ensure_dev_tls.bat" || exit /b 1
set "BAYOU_TLS_CA_FILE=%~dp0tls\dev-client-ca-bundle.pem"
set "GAME_SERVER_CONFIG=%~dp0gameserver.cfg"
if not exist "%GAME_SERVER_CONFIG%" (
    set "GAME_SERVER_CONFIG=%~dp0gameserver.cfg.example"
    echo Using default remote card-server configuration from %~dp0gameserver.cfg.example.
)
"%~dp0build\Debug\gameserver.exe" --config "%GAME_SERVER_CONFIG%"
