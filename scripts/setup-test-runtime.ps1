param(
    [string]$Source = "run",
    [string]$Target = "run-test"
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$src = Join-Path $root $Source
$dst = Join-Path $root $Target

if (-not (Test-Path (Join-Path $src "DinoIsland.exe"))) {
    throw "Source runtime not found: $src\DinoIsland.exe"
}
if (-not (Test-Path (Join-Path $src "DINPUT8.dll"))) {
    throw "Source hook not found: $src\DINPUT8.dll. Run scripts/build-hook.ps1 first."
}

New-Item -ItemType Directory -Force $dst | Out-Null
if (-not (Test-Path (Join-Path $dst "Data"))) {
    New-Item -ItemType Junction -Path (Join-Path $dst "Data") `
        -Target (Join-Path $src "Data") | Out-Null
}

$files = @(
    "DinoIsland.exe", "DinoIsland.exe.orig", "DINPUT8.dll",
    "D3D8.dll", "D3D9.dll", "D3DImm.dll", "DDraw.dll", "dgVoodoo.conf",
    "Dino.ico", "Readme.txt", "SFI.ico", "Dino Island Manual.pdf",
    "Monte Cristo website.url", "Register.url", "Strategy First Website.url"
)
foreach ($file in $files) {
    $from = Join-Path $src $file
    if (Test-Path $from) {
        Copy-Item $from (Join-Path $dst $file) -Force
    }
}

if (-not (Test-Path (Join-Path $dst "save"))) {
    Copy-Item (Join-Path $src "save") (Join-Path $dst "save") -Recurse -Force
}

# dgVoodoo2 can force a fullscreen D3D8 app into a normal window. Disable
# mouse capture too, so automated tests cannot trap the physical cursor.
$confPath = Join-Path $dst "dgVoodoo.conf"
$conf = Get-Content $confPath -Raw
$conf = $conf -replace '(?m)^FullScreenMode\s*=.*$',
    'FullScreenMode                       = false'
$conf = $conf -replace '(?m)^AppControlledScreenMode\s*=.*$',
    'AppControlledScreenMode              = false'
$conf = $conf -replace '(?m)^CaptureMouse\s*=.*$',
    'CaptureMouse                         = false'
$conf = $conf -replace '(?m)^FreeMouse\s*=.*$',
    'FreeMouse                            = true'
$conf = $conf -replace '(?m)^CenterAppWindow\s*=.*$',
    'CenterAppWindow                      = true'
$conf = $conf -replace '(?m)^EnableInactiveAppState\s*=.*$',
    'EnableInactiveAppState               = true'
Set-Content -Path $confPath -Value $conf -Encoding ASCII

Write-Host "Windowed test runtime ready: $dst"
if ($Target -eq "run-window") {
    Write-Host "Launch with: .\scripts\play-windowed-game.ps1"
} else {
    Write-Host "Launch with: .\scripts\play-windowed.ps1"
}
