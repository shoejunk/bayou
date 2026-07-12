@echo off
setlocal
set "TLS_DIR=%~dp0tls"
set "CA_CERT_FILE=%TLS_DIR%\dev-ca-cert.pem"
set "PRODUCTION_CA_FILE=%~dp0deploy\ca\isrg-root-x1.pem"
set "CLIENT_CA_BUNDLE_FILE=%TLS_DIR%\dev-client-ca-bundle.pem"
set "CA_KEY_FILE=%TLS_DIR%\dev-ca-key.pem"
set "CA_CSR_FILE=%TLS_DIR%\dev-ca.csr"
set "CA_EXT_FILE=%TLS_DIR%\dev-ca.ext"
set "CERT_FILE=%TLS_DIR%\dev-server-cert.pem"
set "KEY_FILE=%TLS_DIR%\dev-server-key.pem"
set "CSR_FILE=%TLS_DIR%\dev-server.csr"
set "EXT_FILE=%TLS_DIR%\dev-server.ext"
set "PKI_VERSION_FILE=%TLS_DIR%\dev-pki-v2"

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

if exist "%PKI_VERSION_FILE%" if exist "%CA_CERT_FILE%" if exist "%CA_KEY_FILE%" if exist "%CERT_FILE%" if exist "%KEY_FILE%" (
    "%OPENSSL_EXE%" verify -CAfile "%CA_CERT_FILE%" "%CERT_FILE%" >nul 2>nul
    if not errorlevel 1 goto write_client_ca_bundle
)

echo Creating a localhost-only development CA and TLS certificate...
"%OPENSSL_EXE%" req -new -newkey rsa:3072 -sha256 -nodes ^
    -keyout "%CA_KEY_FILE%" -out "%CA_CSR_FILE%" -subj "/CN=Bayou Development CA"
if errorlevel 1 exit /b 1
> "%CA_EXT_FILE%" (
    echo basicConstraints=critical,CA:TRUE,pathlen:0
    echo keyUsage=critical,keyCertSign,cRLSign
    echo subjectKeyIdentifier=hash
    echo authorityKeyIdentifier=keyid:always
)
"%OPENSSL_EXE%" x509 -req -in "%CA_CSR_FILE%" -days 3650 -sha256 ^
    -signkey "%CA_KEY_FILE%" -out "%CA_CERT_FILE%" -extfile "%CA_EXT_FILE%"
if errorlevel 1 exit /b 1

"%OPENSSL_EXE%" req -new -newkey rsa:3072 -sha256 -nodes ^
    -keyout "%KEY_FILE%" -out "%CSR_FILE%" -subj "/CN=localhost"
if errorlevel 1 exit /b 1

> "%EXT_FILE%" (
    echo basicConstraints=critical,CA:FALSE
    echo subjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1
    echo keyUsage=critical,digitalSignature,keyEncipherment
    echo extendedKeyUsage=serverAuth
)
"%OPENSSL_EXE%" x509 -req -in "%CSR_FILE%" -days 825 -sha256 ^
    -CA "%CA_CERT_FILE%" -CAkey "%CA_KEY_FILE%" -CAcreateserial ^
    -out "%CERT_FILE%" -extfile "%EXT_FILE%"
if errorlevel 1 exit /b 1

del /q "%CA_CSR_FILE%" "%CA_EXT_FILE%" "%CSR_FILE%" "%EXT_FILE%" "%TLS_DIR%\dev-ca-cert.srl" >nul 2>nul
"%OPENSSL_EXE%" verify -CAfile "%CA_CERT_FILE%" "%CERT_FILE%"
if errorlevel 1 exit /b 1
> "%PKI_VERSION_FILE%" echo 2

:write_client_ca_bundle
if not exist "%PRODUCTION_CA_FILE%" (
    echo Production CA certificate was not found at %PRODUCTION_CA_FILE%.
    exit /b 1
)
copy /b "%CA_CERT_FILE%"+"%PRODUCTION_CA_FILE%" "%CLIENT_CA_BUNDLE_FILE%" >nul
if errorlevel 1 (
    echo Could not create the debug client CA bundle at %CLIENT_CA_BUNDLE_FILE%.
    exit /b 1
)

echo Development TLS CA and server certificate created in %TLS_DIR%.
exit /b 0
