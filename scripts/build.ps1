param(
    [string]$BuildDir = "build\mingw",
    [switch]$NoConfigure
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

$cmake = Join-Path $root ".venv\Scripts\cmake.exe"
$ninja = Join-Path $root ".venv\Scripts\ninja.exe"
if (-not (Test-Path $cmake)) { throw "cmake not installed in .venv" }

. (Join-Path $PSScriptRoot "tools.ps1")
$w64 = Find-W64Devkit
if (-not $w64) {
    throw "w64devkit not found under $DinoTools\w64devkit. Run scripts/setup-tools.ps1 (or set DINO_TOOLS)."
}

$sdl3 = Find-Sdl3
if (-not $sdl3) {
    throw "SDL3 not found under $DinoTools\SDL3. Run scripts/setup-tools.ps1, or set SDL3_ROOT to the x86_64-w64-mingw32 prefix."
}
$sdl3Cmake = $sdl3 -replace '\\', '/'

$env:PATH = (Join-Path $root ".venv\Scripts") + ";" + $w64.Bin + ";" + $env:PATH

$build = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir
} else {
    Join-Path $root $BuildDir
}

# A build dir copied from another checkout (e.g. the other PC) keeps that
# machine's absolute path in its cache and CMake refuses it. Drop the cache
# so it reconfigures here; objects are rebuilt, sources are untouched.
$cache = Join-Path $build "CMakeCache.txt"
if (Test-Path $cache) {
    $line = Select-String -Path $cache -Pattern '^CMAKE_CACHEFILE_DIR:INTERNAL=(.*)$' | Select-Object -First 1
    $cached = if ($line) { $line.Matches[0].Groups[1].Value } else { "" }
    $norm = { param($p) ($p -replace '\\', '/').TrimEnd('/').ToLowerInvariant() }
    $here = (Resolve-Path $build).Path
    if ($cached -and ((& $norm $cached) -ne (& $norm $here))) {
        Write-Host "CMake cache was created in $cached; reconfiguring for $here"
        Remove-Item $cache -Force
        Remove-Item (Join-Path $build "CMakeFiles") -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if (-not $NoConfigure -or -not (Test-Path $cache)) {
    & $cmake -S $root -B $build -G Ninja `
        -DCMAKE_BUILD_TYPE=Debug `
        -DCMAKE_CXX_COMPILER="$($w64.Gxx)" `
        -DSDL3_ROOT="$sdl3Cmake"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& $cmake --build $build
exit $LASTEXITCODE
