param(
    [string]$BuildDirectory = "build",
    [string]$OutputDirectory = "dist/client",
    [string]$TlsCaFile = $env:BAYOU_TLS_CA_FILE,
    [string]$TlsServerName = $env:BAYOU_TLS_SERVER_NAME
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildPath = Join-Path $repoRoot $BuildDirectory
$outputPath = Join-Path $repoRoot $OutputDirectory
$releasePath = Join-Path $buildPath "Release"
$zipPath = "$outputPath.zip"

if ([string]::IsNullOrWhiteSpace($TlsCaFile)) {
    $TlsCaFile = Join-Path $PSScriptRoot "ca/isrg-root-x1.pem"
}
if ([string]::IsNullOrWhiteSpace($TlsServerName)) {
    $TlsServerName = "game.gloomthorn.com"
}

if ([string]::IsNullOrWhiteSpace($TlsCaFile) -or !(Test-Path -LiteralPath $TlsCaFile -PathType Leaf)) {
    throw "Set BAYOU_TLS_CA_FILE or pass -TlsCaFile with the PEM CA certificate for the production server."
}
if ([string]::IsNullOrWhiteSpace($TlsServerName)) {
    throw "Set BAYOU_TLS_SERVER_NAME or pass -TlsServerName with the DNS name or IP covered by the production certificate."
}
if ($TlsServerName -notmatch '^[A-Za-z0-9.-]+$') {
    throw "TlsServerName must be a DNS name or IPv4 address."
}

cmake --build $buildPath --config Release --target client
if ($LASTEXITCODE -ne 0) {
    if (Test-Path $outputPath) {
        Remove-Item -Recurse -Force $outputPath
    }
    if (Test-Path $zipPath) {
        Remove-Item -Force $zipPath
    }
    throw "Release client build failed with exit code $LASTEXITCODE; no package was produced."
}

if (Test-Path $outputPath) {
    Remove-Item -Recurse -Force $outputPath
}

New-Item -ItemType Directory -Force $outputPath | Out-Null
Copy-Item (Join-Path $releasePath "SteamTactics.exe") $outputPath
Copy-Item -LiteralPath $TlsCaFile -Destination (Join-Path $outputPath "bayou-ca.pem")

$clientConfig = Get-Content (Join-Path $repoRoot "client_release.cfg") -Raw
$clientConfig = $clientConfig -replace '(?m)^(account_server=)[^:]+(:\d+)$', "`$1$TlsServerName`$2"
$clientConfig = $clientConfig -replace '(?m)^(matchmaking_server=)[^:]+(:\d+)$', "`$1$TlsServerName`$2"
$clientConfig = $clientConfig -replace '(?m)^(card_server=)[^:]+(:\d+)$', "`$1$TlsServerName`$2"
$clientConfig = $clientConfig -replace '(?m)^(game_server_host=).+$', "`$1$TlsServerName"
$escapedServerName = [regex]::Escape($TlsServerName)
foreach ($expected in @(
    "(?m)^account_server=${escapedServerName}:55000$",
    "(?m)^matchmaking_server=${escapedServerName}:55001$",
    "(?m)^card_server=${escapedServerName}:55004$",
    "(?m)^game_server_host=${escapedServerName}$"
)) {
    if ($clientConfig -notmatch $expected) {
        throw "Could not safely rewrite every release client endpoint for TLS identity $TlsServerName."
    }
}
Set-Content -Path (Join-Path $outputPath "client_release.cfg") -Value $clientConfig -NoNewline -Encoding ascii
Copy-Item (Join-Path $repoRoot "assets") $outputPath -Recurse

Get-ChildItem $releasePath -Filter "*.dll" | Copy-Item -Destination $outputPath

if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}
Compress-Archive -Path (Join-Path $outputPath "*") -DestinationPath $zipPath

Write-Host "Packaged release client at $zipPath"
