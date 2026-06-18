@echo off
start "Accounts Service" /D "%~dp0" run_accounts.bat
if exist "%~dp0.env.stripe" (
    start "Stripe Coin Service" /D "%~dp0" run_stripe.bat
) else (
    echo Stripe Coin Service skipped: .env.stripe was not found.
)
start "Card Server Service" /D "%~dp0" run_cardserver.bat
start "Game Server Service" /D "%~dp0" run_gameserver.bat
start "Matchmaking Service" /D "%~dp0" run_matchmaking.bat
