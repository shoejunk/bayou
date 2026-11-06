@echo off
setlocal
cmake --build "%~dp0build" --config Debug --target client audiotest gametest storymodetest
if errorlevel 1 exit /b 1
pushd "%~dp0"
build\Debug\audiotest.exe
if errorlevel 1 goto failed
build\Debug\gametest.exe --movement-only
if errorlevel 1 goto failed
build\Debug\storymodetest.exe
if errorlevel 1 goto failed
build\Debug\SteamTactics.exe --audio-smoke-test=output/audio-integration/playback-smoke.json
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
