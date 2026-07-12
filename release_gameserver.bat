@echo off
set "GAME_SERVER_CONFIG=%~dp0gameserver.cfg"
if not exist "%GAME_SERVER_CONFIG%" (
    set "GAME_SERVER_CONFIG=%~dp0gameserver.cfg.example"
    echo Using default remote card-server configuration from %~dp0gameserver.cfg.example.
)
"%~dp0build\Release\gameserver.exe" --config "%GAME_SERVER_CONFIG%"
