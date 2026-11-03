[CmdletBinding()]
param(
    [string]$SourcePath = (Join-Path $PSScriptRoot '..\assets\bases\gems1.png'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\assets\bases')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

$sourceFullPath = [System.IO.Path]::GetFullPath($SourcePath)
$outputFullPath = [System.IO.Path]::GetFullPath($OutputDirectory)

if (-not [System.IO.File]::Exists($sourceFullPath)) {
    throw "Gem sheet not found: $sourceFullPath"
}

[System.IO.Directory]::CreateDirectory($outputFullPath) | Out-Null

$cellSize = 64
$columnCount = 5
$rowCount = 4
$firstCellX = 32

$sheet = [System.Drawing.Bitmap]::new($sourceFullPath)
try {
    $requiredWidth = $firstCellX + ($columnCount * $cellSize)
    $requiredHeight = $rowCount * $cellSize
    if ($sheet.Width -lt $requiredWidth -or $sheet.Height -lt $requiredHeight) {
        throw "Gem sheet must be at least ${requiredWidth}x${requiredHeight}; found $($sheet.Width)x$($sheet.Height)."
    }

    for ($row = 0; $row -lt $rowCount; $row++) {
        for ($column = 0; $column -lt $columnCount; $column++) {
            $gemNumber = ($row * $columnCount) + $column + 1
            $crop = [System.Drawing.Rectangle]::new(
                $firstCellX + ($column * $cellSize),
                $row * $cellSize,
                $cellSize,
                $cellSize
            )

            $gem = $sheet.Clone($crop, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
            try {
                $outputPath = Join-Path $outputFullPath "gem$gemNumber.png"
                $stream = [System.IO.File]::Open(
                    $outputPath,
                    [System.IO.FileMode]::Create,
                    [System.IO.FileAccess]::Write,
                    [System.IO.FileShare]::None
                )
                try {
                    $gem.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
                }
                finally {
                    $stream.Dispose()
                }

                Write-Output "Wrote $outputPath"
            }
            finally {
                $gem.Dispose()
            }
        }
    }
}
finally {
    $sheet.Dispose()
}
