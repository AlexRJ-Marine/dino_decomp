param(
    [switch]$SkipTests
)

$ErrorActionPreference = "Continue"
$root = Split-Path $PSScriptRoot -Parent
$ok = $true

function Check($name, $condition, $detail) {
    if ($condition) {
        Write-Host ("[ OK ] {0,-22} {1}" -f $name, $detail) -ForegroundColor Green
    } else {
        Write-Host ("[FAIL] {0,-22} {1}" -f $name, $detail) -ForegroundColor Red
        $script:ok = $false
    }
}

$py = Join-Path $root ".venv\Scripts\python.exe"
Check "python venv" (Test-Path $py) $py

if (Test-Path $py) {
    $mods = & $py -c "import importlib.util as u; print(','.join(m for m in ['pyghidra_mcp','pefile','capstone','construct','pytest','PIL'] if u.find_spec(m)))" 2>&1
    Check "python packages" ($LASTEXITCODE -eq 0) $mods
}

foreach ($exe in @("pyghidra-mcp.exe", "mcp-proxy.exe", "pytest.exe", "cmake.exe", "ninja.exe")) {
    $p = Join-Path $root ".venv\Scripts\$exe"
    Check $exe (Test-Path $p) $p
}

$ghidra = Get-ChildItem "C:\Tools" -Directory -Filter "ghidra_*_PUBLIC" -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | Select-Object -First 1
Check "ghidra" ($null -ne $ghidra) ($(if ($ghidra) { $ghidra.FullName } else { "not found under C:\Tools" }))

$gxx = Get-ChildItem "C:\Tools\w64devkit" -Recurse -Filter "g++.exe" -ErrorAction SilentlyContinue |
    Select-Object -First 1
Check "g++ (w64devkit)" ($null -ne $gxx) ($(if ($gxx) { $gxx.FullName } else { "not found under C:\Tools\w64devkit" }))

$sdl3 = Get-ChildItem "C:\Tools\SDL3" -Directory -Filter "SDL3-*" -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | Select-Object -First 1
$sdl3Prefix = if ($sdl3) { Join-Path $sdl3.FullName "x86_64-w64-mingw32" } else { "" }
$sdl3Ok = $sdl3Prefix -and (Test-Path (Join-Path $sdl3Prefix "include\SDL3\SDL.h"))
Check "SDL3 (MinGW)" $sdl3Ok ($(if ($sdl3Ok) { $sdl3Prefix } else { "not found (run scripts/setup-sdl3.ps1)" }))

$jdk = Get-ChildItem "C:\Tools\jdk25" -Directory -Filter "jdk-*" -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | Select-Object -First 1
$javaExe = if ($jdk) { Join-Path $jdk.FullName "bin\java.exe" } else { (Get-Command java -ErrorAction SilentlyContinue).Source }
$javaVer = if ($javaExe) { (& $javaExe -version 2>&1 | Select-Object -First 1) } else { "" }
Check "java 25 (JDK)" ($jdk -and $javaVer -match '"25\.') ($(if ($jdk) { "$javaVer  $($jdk.FullName)" } else { "JDK 25 not found under C:\Tools\jdk25" }))

$game = Join-Path $root "game\%MAINDIR%\DinoIsland.exe"
Check "game exe" (Test-Path $game) $game

$listening = Get-NetTCPConnection -LocalPort 8000 -State Listen -ErrorAction SilentlyContinue
Check "ghidra mcp :8000" ($null -ne $listening) ($(if ($listening) { "listening" } else { "not running (run scripts/start-ghidra.ps1)" }))

if (-not $SkipTests -and (Test-Path (Join-Path $root "tests"))) {
    Write-Host "`nRunning Python tests..." -ForegroundColor Cyan
    & (Join-Path $root ".venv\Scripts\pytest.exe") -q (Join-Path $root "tests")
    if ($LASTEXITCODE -ne 0) { $ok = $false }
}

if ($ok) {
    Write-Host "`nEnvironment OK." -ForegroundColor Green
    exit 0
} else {
    Write-Host "`nEnvironment incomplete; see failures above." -ForegroundColor Yellow
    exit 1
}
