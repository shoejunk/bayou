@echo off
setlocal
set "ROOT=%~dp0.."
call "%ROOT%\tools\run_animation_halo_cleanup.bat" %*
