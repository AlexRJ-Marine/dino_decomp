$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

$cmake = Join-Path $root ".venv\Scripts\cmake.exe"
$ninja = Join-Path $root ".venv\Scripts\ninja.exe"
if (-not (Test-Path $cmake)) { throw "cmake not installed in .venv" }

$gxx = Get-ChildItem "C:\Tools\w64devkit" -Recurse -Filter "g++.exe" -ErrorAction SilentlyContinue |
    Select-Object -First 1
$gcc = Get-ChildItem "C:\Tools\w64devkit" -Recurse -Filter "gcc.exe" -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $gxx -or -not $gcc) {
    throw "w64devkit not found. Download and extract it to C:\Tools\w64devkit."
}

$env:PATH = (Join-Path $root ".venv\Scripts") + ";" + (Split-Path $gxx.FullName) + ";" + $env:PATH

$build = Join-Path $root "build\mingw"
& $cmake -S $root -B $build -G Ninja `
    -DCMAKE_BUILD_TYPE=Debug `
    -DCMAKE_C_COMPILER="$($gcc.FullName)" `
    -DCMAKE_CXX_COMPILER="$($gxx.FullName)"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build $build
exit $LASTEXITCODE
