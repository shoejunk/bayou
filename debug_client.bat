@echo off
set "BAYOU_TLS_CA_FILE=%~dp0deploy\ca\isrg-root-x1.pem"
"%~dp0build\Debug\SteamTactics.exe"
