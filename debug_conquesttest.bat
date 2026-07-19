@echo off
cmake --build "%~dp0build" --config Debug --target conquesttest || exit /b 1
"%~dp0build\Debug\conquesttest.exe"
