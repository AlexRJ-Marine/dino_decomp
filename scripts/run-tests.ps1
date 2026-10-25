$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$pytest = Join-Path $root ".venv\Scripts\pytest.exe"
if (-not (Test-Path $pytest)) { throw "pytest not installed in .venv" }

Push-Location $root
try {
    & $pytest -q tests
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
