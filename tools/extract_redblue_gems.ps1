[CmdletBinding()]
param(
    [string]$SourcePath = (Join-Path $PSScriptRoot '..\assets\bases\redbluegems.png'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\assets\bases')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

$sourceFullPath = [System.IO.Path]::GetFullPath($SourcePath)
$outputFullPath = [System.IO.Path]::GetFullPath($OutputDirectory)
if (-not [System.IO.File]::Exists($sourceFullPath)) {
    throw "Gem source not found: $sourceFullPath"
}
[System.IO.Directory]::CreateDirectory($outputFullPath) | Out-Null

# Alpha-component bounds measured from redbluegems.png. Each gem is scaled to
# the 62-pixel visible height used by the upright rarity gems, centered in the
# frame, and bottom-aligned at y=64.
$specifications = @(
    @{ Name = 'gem1.png'; Bounds = [System.Drawing.Rectangle]::new(9, 6, 60, 79) },
    @{ Name = 'gem2.png'; Bounds = [System.Drawing.Rectangle]::new(81, 7, 60, 78) }
)

$source = [System.Drawing.Bitmap]::new($sourceFullPath)
try {
    foreach ($specification in $specifications) {
        $bounds = $specification.Bounds
        if ($bounds.Right -gt $source.Width -or $bounds.Bottom -gt $source.Height) {
            throw "Crop for $($specification.Name) exceeds the $($source.Width)x$($source.Height) source."
        }

        $targetHeight = 62
        $targetWidth = [Math]::Round($bounds.Width * $targetHeight / $bounds.Height)
        $targetX = [Math]::Floor((64 - $targetWidth) / 2)
        $targetY = 64 - $targetHeight
        $frame = [System.Drawing.Bitmap]::new(
            64,
            64,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
        )
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($frame)
            try {
                $graphics.Clear([System.Drawing.Color]::Transparent)
                $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
                $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                $graphics.DrawImage(
                    $source,
                    [System.Drawing.Rectangle]::new($targetX, $targetY, $targetWidth, $targetHeight),
                    $bounds,
                    [System.Drawing.GraphicsUnit]::Pixel
                )
            }
            finally {
                $graphics.Dispose()
            }

            $outputPath = Join-Path $outputFullPath $specification.Name
            $stream = [System.IO.File]::Open(
                $outputPath,
                [System.IO.FileMode]::Create,
                [System.IO.FileAccess]::Write,
                [System.IO.FileShare]::None
            )
            try {
                $frame.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
            }
            finally {
                $stream.Dispose()
            }
            Write-Output "Wrote $outputPath (${targetWidth}x${targetHeight} at ${targetX},${targetY})"
        }
        finally {
            $frame.Dispose()
        }
    }
}
finally {
    $source.Dispose()
}
