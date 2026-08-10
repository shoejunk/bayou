@echo off
setlocal
set "ROOT=%~dp0.."
set "PYTHON_EXE="

if defined BAYOU_PYTHON if exist "%BAYOU_PYTHON%" set "PYTHON_EXE=%BAYOU_PYTHON%"
if not defined PYTHON_EXE if exist "%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" set "PYTHON_EXE=%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
if not defined PYTHON_EXE for %%I in (python.exe) do if not "%%~$PATH:I"=="" set "PYTHON_EXE=%%~$PATH:I"

if not defined PYTHON_EXE (
  echo Could not find Python. Set BAYOU_PYTHON to a Python executable with Pillow and NumPy installed.
  exit /b 1
)

"%PYTHON_EXE%" "%ROOT%\tools\animation_halo_cleanup.py" --root "%ROOT%" %*
