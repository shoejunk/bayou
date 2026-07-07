@echo off
setlocal EnableExtensions

set "REMOTE_USER=opc"
set "REMOTE_HOST=147.224.11.162"
set "REMOTE=%REMOTE_USER%@%REMOTE_HOST%"
set "LOCAL_DB=%~dp0cards.db"
set "REMOTE_DB=/var/lib/bayou/shared/cards.db"

if not exist "%LOCAL_DB%" (
    echo Missing local database: %LOCAL_DB%
    exit /b 1
)

where ssh >nul 2>nul
if errorlevel 1 (
    echo Missing required command: ssh
    exit /b 1
)

where scp >nul 2>nul
if errorlevel 1 (
    echo Missing required command: scp
    exit /b 1
)

for /f %%I in ('powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-Date -Format yyyyMMddHHmmss"') do set "STAMP=%%I"
set "REMOTE_TMP=/tmp/cards.db.local-%STAMP%"
set "REMOTE_BACKUP=%REMOTE_DB%.backup-%STAMP%"

echo Local database:
echo   %LOCAL_DB%
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-FileHash -Algorithm SHA256 '%LOCAL_DB%' | ForEach-Object { '  SHA256: ' + $_.Hash.ToLower() }"
if errorlevel 1 exit /b 1

echo.
echo Uploading to %REMOTE%:%REMOTE_TMP%...
scp -o BatchMode=yes "%LOCAL_DB%" "%REMOTE%:%REMOTE_TMP%"
if errorlevel 1 exit /b 1

echo.
echo Replacing %REMOTE_DB% on %REMOTE%...
ssh -o BatchMode=yes "%REMOTE%" "set -euo pipefail; sudo -n systemctl stop bayou-cardserver bayou-accounts bayou-gameserver bayou-matchmaking; trap 'sudo -n systemctl start bayou-accounts bayou-cardserver bayou-gameserver bayou-matchmaking' EXIT; sudo -n cp -a %REMOTE_DB% %REMOTE_BACKUP%; sudo -n install -m 0600 -o bayou -g bayou %REMOTE_TMP% %REMOTE_DB%; sudo -n rm -f %REMOTE_TMP%; sudo -n systemctl start bayou-accounts bayou-cardserver bayou-gameserver bayou-matchmaking; trap - EXIT; sudo -n sha256sum %REMOTE_DB%; sudo -n ls -l %REMOTE_DB% %REMOTE_BACKUP%; sudo -n sqlite3 %REMOTE_DB% 'PRAGMA integrity_check; SELECT count(*) AS cards FROM cards; SELECT count(*) AS actions FROM actions;'; systemctl is-active bayou-accounts; systemctl is-active bayou-cardserver; systemctl is-active bayou-gameserver; systemctl is-active bayou-matchmaking"
if errorlevel 1 (
    echo.
    echo Push failed. Check the remote service state and any temporary file at %REMOTE%:%REMOTE_TMP%.
    exit /b 1
)

echo.
echo Remote card database replaced successfully.
echo Backup created:
echo   %REMOTE_BACKUP%
