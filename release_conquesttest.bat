@echo off
cmake --build "%~dp0build" --config Release --target conquesttest || exit /b 1
"%~dp0build\Release\conquesttest.exe"
