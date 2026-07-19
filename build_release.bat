@echo off
cmake --build "%~dp0build" --config Release --target client accounts matchmaking gameserver gametest conquesttest cardserver
