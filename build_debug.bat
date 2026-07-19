@echo off
cmake --build "%~dp0build" --config Debug --target client accounts matchmaking gameserver gametest conquesttest cardserver
