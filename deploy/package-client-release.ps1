param(
    [string]$BuildDirectory = "build",
    [string]$OutputDirectory = "dist/client"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildPath = Join-Path $repoRoot $BuildDirectory
$outputPath = Join-Path $repoRoot $OutputDirectory
$releasePath = Join-Path $buildPath "Release"

cmake --build $buildPath --config Release --target client

if (Test-Path $outputPath) {
    Remove-Item -Recurse -Force $outputPath
}

New-Item -ItemType Directory -Force $outputPath | Out-Null
Copy-Item (Join-Path $releasePath "SteamTactics.exe") $outputPath
Copy-Item (Join-Path $repoRoot "client.cfg") $outputPath
Copy-Item (Join-Path $repoRoot "assets") $outputPath -Recurse

Get-ChildItem $releasePath -Filter "*.dll" | Copy-Item -Destination $outputPath

$zipPath = "$outputPath.zip"
if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}
Compress-Archive -Path (Join-Path $outputPath "*") -DestinationPath $zipPath

Write-Host "Packaged release client at $zipPath"
