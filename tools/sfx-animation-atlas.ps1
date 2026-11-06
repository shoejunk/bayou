$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$destination = Join-Path (Get-Location) 'output/sfx-audit'
$files = @(Get-ChildItem -LiteralPath 'assets/animations' -File | Where-Object BaseName -Match '-attack\d*$' | Sort-Object Name)
$font = [System.Drawing.Font]::new('Arial', 12)
$brush = [System.Drawing.Brushes]::White
for ($page = 0; $page -lt [math]::Ceiling($files.Count / 25); $page++) {
    $subset = @($files | Select-Object -Skip ($page * 25) -First 25)
    $canvas = [System.Drawing.Bitmap]::new(1100, $subset.Count * 88)
    $graphics = [System.Drawing.Graphics]::FromImage($canvas)
    $graphics.Clear([System.Drawing.Color]::FromArgb(35, 40, 43))
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    for ($row = 0; $row -lt $subset.Count; $row++) {
        $file = $subset[$row]
        $strip = [System.Drawing.Image]::FromFile($file.FullName)
        $frames = [int]($strip.Width / $strip.Height)
        $graphics.DrawString($file.BaseName, $font, $brush, 5, ($row * 88 + 31))
        for ($sample = 0; $sample -lt 6; $sample++) {
            $frame = [math]::Min($frames - 1, [int][math]::Floor($sample * $frames / 6))
            $target = [System.Drawing.Rectangle]::new((285 + $sample * 130), ($row * 88), 88, 88)
            $source = [System.Drawing.Rectangle]::new(($frame * $strip.Height), 0, $strip.Height, $strip.Height)
            $graphics.DrawImage($strip, $target, $source, [System.Drawing.GraphicsUnit]::Pixel)
        }
        $strip.Dispose()
    }
    $canvas.Save((Join-Path $destination ('attack-atlas-' + ($page + 1) + '.png')))
    $graphics.Dispose()
    $canvas.Dispose()
}
$font.Dispose()
Write-Output ('Inspected source sequences: ' + $files.Count)
