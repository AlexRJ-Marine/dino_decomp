param(
    [switch]$Fast,
    [string]$Exe = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

if ($Exe) {
    & (Join-Path $PSScriptRoot "test.ps1") -Exe $Exe -Fast:$Fast
    exit $LASTEXITCODE
}

$pytest = Join-Path $root ".venv\Scripts\pytest.exe"
if (-not (Test-Path $pytest)) { throw "pytest not installed in .venv" }

$pytestArgs = @("-q", "tests")
if ($Fast) { $pytestArgs += @("-m", "not slow") }

Push-Location $root
try {
    & $pytest @pytestArgs
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
