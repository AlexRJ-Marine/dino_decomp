$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

$gxx = Get-ChildItem "C:\Tools\w64devkit" -Recurse -Filter "g++.exe" -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $gxx) {
    throw "w64devkit g++ not found under C:\Tools\w64devkit"
}

$source = Join-Path $root "src\hook\dinput8_proxy.cpp"
$outDir = Join-Path $root "run"
$out = Join-Path $outDir "DINPUT8.dll"
New-Item -ItemType Directory -Force $outDir | Out-Null

& $gxx.FullName -m32 -std=c++20 -O2 -shared `
    -static-libgcc -static-libstdc++ `
    "-Wl,--kill-at" `
    -o $out $source
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Built $out"
$objdump = Join-Path (Split-Path $gxx.FullName) "objdump.exe"
if (Test-Path $objdump) {
    & $objdump -p $out | Select-String -Pattern "DirectInput8Create|DLL Name" | ForEach-Object { $_.Line }
}
