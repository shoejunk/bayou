@echo off
set "SERVER_CONFIG=%~dp0gameserver.cfg"
if not exist "%SERVER_CONFIG%" set "SERVER_CONFIG=%~dp0gameserver.cfg.example"
"%~dp0build\Release\accounts.exe" --config "%SERVER_CONFIG%"
