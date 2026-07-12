@echo off
start "Accounts Service" /D "%~dp0" release_accounts.bat
if exist "%~dp0.env.stripe" (
    start "Stripe Coin Service" /D "%~dp0" run_stripe.bat
) else (
    echo Stripe Coin Service skipped: .env.stripe was not found.
)
echo Card Server Service skipped: release clients and game servers use the configured authoritative card server.
start "Game Server Service" /D "%~dp0" release_gameserver.bat
start "Matchmaking Service" /D "%~dp0" release_matchmaking.bat
