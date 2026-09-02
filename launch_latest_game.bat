@echo off
setlocal
cd /d "%~dp0"

for /f "delims=" %%B in ('git branch --show-current') do set "CURRENT_BRANCH=%%B"
if /i not "%CURRENT_BRANCH%"=="main" (
    echo This launcher must be run from the main branch.
    pause
    exit /b 1
)

echo Updating the game from GitHub...
git pull --ff-only origin main
if errorlevel 1 (
    echo.
    echo The game was not updated. Check your GitHub sign-in, internet connection,
    echo or local changes, then try again.
    pause
    exit /b 1
)

echo.
echo Building the latest client...
call "%~dp0build_debug_client.bat"
if errorlevel 1 (
    echo.
    echo The latest client could not be built, so the game was not started.
    pause
    exit /b 1
)

if not exist "%~dp0build\Debug\SteamTactics.exe" (
    echo.
    echo The client executable was not found after the build.
    pause
    exit /b 1
)

echo.
echo Starting the latest game...
call "%~dp0debug_client.bat"
exit /b %errorlevel%
