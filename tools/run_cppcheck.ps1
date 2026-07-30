[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$cppcheckCandidates = @()

if ($env:CPPCHECK_EXE)
{
    $cppcheckCandidates += $env:CPPCHECK_EXE
}

$pathCommand = Get-Command cppcheck.exe -ErrorAction SilentlyContinue
if ($pathCommand)
{
    $cppcheckCandidates += $pathCommand.Source
}

$localApplicationData = [Environment]::GetFolderPath(
    [Environment+SpecialFolder]::LocalApplicationData)
if ($localApplicationData)
{
    $cppcheckCandidates += Join-Path $localApplicationData "Programs\Cppcheck\cppcheck.exe"
}

if ($env:ProgramFiles)
{
    $cppcheckCandidates += Join-Path $env:ProgramFiles "Cppcheck\cppcheck.exe"
}

$programFilesX86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
if ($programFilesX86)
{
    $cppcheckCandidates += Join-Path $programFilesX86 "Cppcheck\cppcheck.exe"
}

$cppcheckExecutable = $cppcheckCandidates |
    Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } |
    Select-Object -First 1

if (-not $cppcheckExecutable)
{
    Write-Error @"
Cppcheck was not found. Install Cppcheck 2.21 or newer, add cppcheck.exe to
PATH, or set CPPCHECK_EXE to its full path. See CPPCHECK.md.
"@
    exit 1
}

$outputDirectory = Join-Path $repositoryRoot "output\cppcheck"
$cacheDirectory = Join-Path $outputDirectory "cache"
$xmlReport = Join-Path $outputDirectory "cppcheck.xml"
$summaryReport = Join-Path $outputDirectory "cppcheck-summary.txt"
$stdoutLog = Join-Path $outputDirectory "cppcheck.log"

New-Item -ItemType Directory -Force -Path $cacheDirectory | Out-Null

$analyzedExtensions = @(".c", ".cc", ".cpp", ".cxx", ".cppm", ".ixx")
$sourceFiles = Get-ChildItem -LiteralPath (Join-Path $repositoryRoot "src") -Recurse -File |
    Where-Object { $analyzedExtensions -contains $_.Extension.ToLowerInvariant() } |
    Sort-Object FullName |
    ForEach-Object {
        $_.FullName.Substring($repositoryRoot.Length + 1)
    }

if (-not $sourceFiles)
{
    Write-Error "No C or C++ source files were found under $repositoryRoot\src."
    exit 1
}

$cppcheckArguments = @(
    "--enable=warning,performance,portability",
    "--check-level=normal",
    "--std=c++20",
    "--platform=win64",
    "--library=windows",
    "--inline-suppr",
    "--suppress=missingIncludeSystem",
    "--suppress=unmatchedSuppression",
    "--suppressions-list=cppcheck-suppressions.txt",
    "--cppcheck-build-dir=output\cppcheck\cache",
    "--error-exitcode=2",
    "--xml",
    "--xml-version=2",
    "-D_WIN32",
    "-D_WIN64",
    "-D_WIN32_WINNT=0x0A00",
    "-DASIO_STANDALONE",
    "-DASIO_NO_DEPRECATED",
    "-Isrc\shared",
    "-Isrc\client",
    "-Isrc\accounts",
    "-Isrc\gameserver",
    "-j4"
) + $sourceFiles

Write-Host "Running $(& $cppcheckExecutable --version) on $($sourceFiles.Count) Bayou source files..."

$process = Start-Process `
    -FilePath $cppcheckExecutable `
    -ArgumentList $cppcheckArguments `
    -WorkingDirectory $repositoryRoot `
    -RedirectStandardOutput $stdoutLog `
    -RedirectStandardError $xmlReport `
    -NoNewWindow `
    -Wait `
    -PassThru

if (-not (Test-Path -LiteralPath $xmlReport -PathType Leaf))
{
    Write-Error "Cppcheck did not create $xmlReport."
    exit 1
}

try
{
    [xml]$report = Get-Content -LiteralPath $xmlReport -Raw
}
catch
{
    if (Test-Path -LiteralPath $stdoutLog)
    {
        Get-Content -LiteralPath $stdoutLog | Write-Host
    }
    Write-Error "Cppcheck produced an invalid XML report: $($_.Exception.Message)"
    exit 1
}

$issues = @($report.SelectNodes("/results/errors/error"))
$summaryLines = @(
    "Cppcheck report: $xmlReport",
    "Source files: $($sourceFiles.Count)",
    "Findings: $($issues.Count)"
)

if ($issues.Count -gt 0)
{
    $severitySummary = $issues |
        Group-Object { $_.GetAttribute("severity") } |
        Sort-Object Name |
        ForEach-Object { "$($_.Name)=$($_.Count)" }
    $summaryLines += "Severities: $($severitySummary -join ', ')"
    $summaryLines += ""

    foreach ($issue in $issues)
    {
        $location = $issue.SelectSingleNode("location")
        $file = if ($location) { $location.GetAttribute("file") } else { "<unknown>" }
        $line = if ($location) { $location.GetAttribute("line") } else { "0" }
        $column = if ($location) { $location.GetAttribute("column") } else { "0" }
        $summaryLines += "{0}({1},{2}): {3}: {4} [{5}]" -f `
            $file,
            $line,
            $column,
            $issue.GetAttribute("severity"),
            $issue.GetAttribute("msg"),
            $issue.GetAttribute("id")
    }
}

$summaryLines | Set-Content -LiteralPath $summaryReport -Encoding UTF8
$summaryLines | ForEach-Object { Write-Host $_ }

if ($process.ExitCode -ne 0)
{
    if ($issues.Count -eq 0 -and (Test-Path -LiteralPath $stdoutLog))
    {
        Get-Content -LiteralPath $stdoutLog | Write-Host
    }
    [Console]::Error.WriteLine(
        "Cppcheck failed with exit code $($process.ExitCode).")
    exit $process.ExitCode
}

Write-Host "Cppcheck passed."
exit 0
