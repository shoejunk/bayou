@echo off
setlocal
pushd "%~dp0"

python src\payments\run_stripe_local.py
set "exit_code=%ERRORLEVEL%"

popd
exit /b %exit_code%
