@echo off
start "Accounts Service" /D "%~dp0" run_accounts.bat
start "Card Server Service" /D "%~dp0" run_cardserver.bat
start "Game Server Service" /D "%~dp0" run_gameserver.bat
start "Matchmaking Service" /D "%~dp0" run_matchmaking.bat
