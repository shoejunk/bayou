$ErrorActionPreference = 'Stop'
$sfxRoot = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) 'assets/audio/sfx/masters'))
$plan = Get-Content -LiteralPath 'output/sfx-audit/downloads.json' -Raw | ConvertFrom-Json
foreach ($item in $plan) {
    $target = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $item.file))
    if (-not $target.StartsWith($sfxRoot + [System.IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Download outside sound library' }
    if ((Test-Path -LiteralPath $target) -and (Get-Item -LiteralPath $target).Length -gt 1024) { Write-Output ('Exists: ' + $item.id); continue }
    for ($attempt = 1; $attempt -le 3; $attempt++) {
        try {
            Invoke-WebRequest -Uri $item.url -OutFile ($target + '.partial') -TimeoutSec 45
            if ((Get-Item -LiteralPath ($target + '.partial')).Length -lt 1024) { throw 'Incomplete audio download' }
            Move-Item -LiteralPath ($target + '.partial') -Destination $target -Force
            Write-Output ('Saved: ' + $item.id)
            break
        } catch { if ($attempt -eq 3) { throw } }
    }
}
