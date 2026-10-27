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

. (Join-Path $PSScriptRoot "tools.ps1")
Write-Host ("[INFO] {0,-22} {1}" -f "tools root", "$DinoTools$(if (-not $env:DINO_TOOLS) { ' (default; set DINO_TOOLS to change)' })")

$ghidra = Find-Ghidra
Check "ghidra" ($null -ne $ghidra) ($(if ($ghidra) { $ghidra } else { "not found under $DinoTools (run scripts/setup-tools.ps1)" }))

$w64 = Find-W64Devkit
Check "g++ (w64devkit)" ($null -ne $w64) ($(if ($w64) { $w64.Gxx } else { "not found under $DinoTools\w64devkit (run scripts/setup-tools.ps1)" }))

$sdl3Prefix = Find-Sdl3
Check "SDL3 (MinGW)" ($null -ne $sdl3Prefix) ($(if ($sdl3Prefix) { $sdl3Prefix } else { "not found under $DinoTools\SDL3 (run scripts/setup-tools.ps1)" }))

$jdk = Find-Jdk25
$javaVer = if ($jdk) { (& (Join-Path $jdk "bin\java.exe") -version 2>&1 | Select-Object -First 1) } else { "" }
Check "java 25 (JDK)" ($null -ne $jdk) ($(if ($jdk) { "$javaVer  $jdk" } else { "JDK 25 not found under $DinoTools\jdk25 or JAVA_HOME" }))

# Commits should carry each contributor's own identity (global git config).
# A repo-local user.name usually came along with a copied checkout.
$gitWho = "$(git -C $root config user.name) <$(git -C $root config user.email)>"
$localName = git -C $root config --local user.name
if ($localName) {
    Write-Host ("[WARN] {0,-22} {1}" -f "git identity", "$gitWho is set in this repo's .git/config; if that is not you, run: git config --local --unset user.name; git config --local --unset user.email") -ForegroundColor Yellow
} else {
    Write-Host ("[INFO] {0,-22} {1}" -f "git identity", "$gitWho (from your global git config)")
}

$x32 = Get-X32dbgHeadless
Check "x32dbg headless" (Test-Path $x32) ($(if (Test-Path $x32) { $x32 } else { "not found at $x32 (harness only; run scripts/setup-tools.ps1)" }))

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
