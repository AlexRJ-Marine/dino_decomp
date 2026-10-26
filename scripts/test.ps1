param(
    [string]$Exe = "",
    [switch]$Fast,
    [string]$Pattern = "",
    [string]$File = "",
    [switch]$VerboseOutput
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$pytest = Join-Path $root ".venv\Scripts\pytest.exe"
if (-not (Test-Path $pytest)) { throw "pytest not installed in .venv" }

if (-not $Exe) {
    $candidates = @(
        (Join-Path $root "build\mingw\dino_viewer.exe"),
        (Join-Path $root "build\mingw_check\dino_viewer.exe")
    )
    $Exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Exe -or -not (Test-Path $Exe)) {
    throw "dino_viewer.exe not found; build first with scripts/build.ps1 -BuildDir build/<lane>"
}
$env:DINO_VIEWER_EXE = (Resolve-Path $Exe).Path

$pytestArgs = @()
if ($VerboseOutput) { $pytestArgs += "-v" } else { $pytestArgs += "-q" }
if ($File) { $pytestArgs += $File } else { $pytestArgs += "tests" }
if ($Fast) { $pytestArgs += @("-m", "not slow") }
if ($Pattern) { $pytestArgs += @("-k", $Pattern) }

Push-Location $root
try {
    Write-Host "DINO_VIEWER_EXE=$env:DINO_VIEWER_EXE"
    & $pytest @pytestArgs
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
