@echo off
setlocal
set "ROOT=%~dp0.."
set "CSC="
for /f "delims=" %%I in ('dir /b /s "%WINDIR%\Microsoft.NET\Framework64\csc.exe" 2^>nul') do set "CSC=%%I"
if "%CSC%"=="" (
  echo Could not find csc.exe under %WINDIR%\Microsoft.NET\Framework64.
  exit /b 1
)
"%CSC%" /nologo /r:System.Drawing.dll /out:"%TEMP%\PngAlphaRepair.exe" "%ROOT%\tools\PngAlphaRepair.cs"
if errorlevel 1 exit /b 1
"%TEMP%\PngAlphaRepair.exe" --root "%ROOT%" %*
