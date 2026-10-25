param(
    [Parameter(Mandatory = $true)]
    [string]$Target
)

$root = Split-Path $PSScriptRoot -Parent
$py = Join-Path $root ".venv\Scripts\python.exe"
$patcher = Join-Path $root "tools\patch_nocd.py"

& $py $patcher --apply $Target
exit $LASTEXITCODE
