$ErrorActionPreference = "Continue"
$root = Split-Path $PSScriptRoot -Parent
$cli = Join-Path $root ".venv\Scripts\pyghidra-mcp-cli.exe"

if (-not (Test-Path $cli)) {
    Write-Host "pyghidra-mcp-cli is not installed. Install it with:" -ForegroundColor Yellow
    Write-Host "  .\.venv\Scripts\pip.exe install pyghidra-mcp-cli"
    exit 1
}

if (-not (Get-NetTCPConnection -LocalPort 8000 -State Listen -ErrorAction SilentlyContinue)) {
    Write-Host "Ghidra MCP is not running. Start it with scripts/start-ghidra.ps1." -ForegroundColor Yellow
    exit 1
}

& $cli list binaries
exit $LASTEXITCODE
