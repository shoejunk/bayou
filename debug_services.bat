@echo off
setlocal
rem Prepare shared TLS files before the service processes start reading them.
call "%~dp0ensure_dev_tls.bat" || exit /b 1
set "BAYOU_DEV_TLS_READY=1"

start "Accounts Service" /D "%~dp0" debug_accounts.bat
if exist "%~dp0.env.stripe" (
    start "Stripe Coin Service" /D "%~dp0" run_stripe.bat
) else (
    echo Stripe Coin Service skipped: .env.stripe was not found.
)
echo Card Server Service skipped: the debug client and game server use the configured authoritative card server.
start "Game Server Service" /D "%~dp0" debug_gameserver.bat
start "Matchmaking Service" /D "%~dp0" debug_matchmaking.bat
