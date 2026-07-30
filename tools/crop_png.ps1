# Crops a region out of a PNG at native resolution, optionally magnifying it, so
# UI details can be inspected without the whole-frame downscale.
param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Destination,
    [Parameter(Mandatory = $true)][int]$X,
    [Parameter(Mandatory = $true)][int]$Y,
    [Parameter(Mandatory = $true)][int]$Width,
    [Parameter(Mandatory = $true)][int]$Height,
    [int]$Zoom = 1
)

Add-Type -AssemblyName System.Drawing

$image = [System.Drawing.Image]::FromFile((Resolve-Path $Source))
try
{
    $cropWidth = [Math]::Min($Width, $image.Width - $X)
    $cropHeight = [Math]::Min($Height, $image.Height - $Y)
    $outWidth = $cropWidth * $Zoom
    $outHeight = $cropHeight * $Zoom

    $output = New-Object System.Drawing.Bitmap($outWidth, $outHeight)
    $graphics = [System.Drawing.Graphics]::FromImage($output)
    try
    {
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
        $sourceRect = New-Object System.Drawing.Rectangle($X, $Y, $cropWidth, $cropHeight)
        $destRect = New-Object System.Drawing.Rectangle(0, 0, $outWidth, $outHeight)
        $graphics.DrawImage($image, $destRect, $sourceRect, [System.Drawing.GraphicsUnit]::Pixel)
    }
    finally
    {
        $graphics.Dispose()
    }

    $output.Save($Destination, [System.Drawing.Imaging.ImageFormat]::Png)
    $output.Dispose()
    Write-Output "wrote $Destination ($outWidth x $outHeight)"
}
finally
{
    $image.Dispose()
}
