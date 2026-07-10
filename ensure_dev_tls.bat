@echo off
setlocal
set "TLS_DIR=%~dp0tls"
set "CERT_FILE=%TLS_DIR%\dev-server-cert.pem"
set "KEY_FILE=%TLS_DIR%\dev-server-key.pem"

if exist "%CERT_FILE%" if exist "%KEY_FILE%" exit /b 0

if not exist "%TLS_DIR%" mkdir "%TLS_DIR%"

set "OPENSSL_EXE=openssl.exe"
where openssl.exe >nul 2>nul
if errorlevel 1 (
    if exist "C:\Program Files\Git\usr\bin\openssl.exe" (
        set "OPENSSL_EXE=C:\Program Files\Git\usr\bin\openssl.exe"
    ) else (
        echo OpenSSL was not found. Install OpenSSL or Git for Windows to create development TLS certificates.
        exit /b 1
    )
)

echo Creating a localhost-only development TLS certificate...
"%OPENSSL_EXE%" req -x509 -newkey rsa:3072 -sha256 -nodes -days 825 ^
    -keyout "%KEY_FILE%" -out "%CERT_FILE%" ^
    -subj "/CN=localhost" ^
    -addext "subjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1" ^
    -addext "keyUsage=critical,digitalSignature,keyEncipherment" ^
    -addext "extendedKeyUsage=serverAuth"
if errorlevel 1 exit /b 1

echo Development TLS certificate created in %TLS_DIR%.
exit /b 0
