param(
    [string]$BuildDir = "build\mingw",
    [switch]$NoConfigure
)

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

$sdl3 = $env:SDL3_ROOT
if (-not $sdl3) {
    $candidates = Get-ChildItem "C:\Tools\SDL3" -Directory -Filter "SDL3-*" -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending
    foreach ($candidate in $candidates) {
        $prefix = Join-Path $candidate.FullName "x86_64-w64-mingw32"
        if (Test-Path (Join-Path $prefix "include\SDL3\SDL.h")) {
            $sdl3 = $prefix
            break
        }
    }
}
if (-not $sdl3 -or -not (Test-Path (Join-Path $sdl3 "include\SDL3\SDL.h"))) {
    throw "SDL3 not found. Run scripts/setup-sdl3.ps1, or set SDL3_ROOT to the x86_64-w64-mingw32 prefix."
}
$sdl3Cmake = $sdl3 -replace '\\', '/'

$env:PATH = (Join-Path $root ".venv\Scripts") + ";" + (Split-Path $gxx.FullName) + ";" + $env:PATH

$build = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir
} else {
    Join-Path $root $BuildDir
}
if (-not $NoConfigure -or -not (Test-Path (Join-Path $build "CMakeCache.txt"))) {
    & $cmake -S $root -B $build -G Ninja `
        -DCMAKE_BUILD_TYPE=Debug `
        -DCMAKE_C_COMPILER="$($gcc.FullName)" `
        -DCMAKE_CXX_COMPILER="$($gxx.FullName)" `
        -DSDL3_ROOT="$sdl3Cmake"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& $cmake --build $build
exit $LASTEXITCODE
