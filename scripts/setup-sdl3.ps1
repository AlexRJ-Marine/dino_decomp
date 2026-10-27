param(
    [string]$Version = "3.4.18",
    [string]$Destination = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tools.ps1")
if (-not $Destination) { $Destination = Join-Path $DinoTools "SDL3" }
$ProgressPreference = "SilentlyContinue"

$zip = Join-Path $env:TEMP "SDL3-devel-$Version-mingw.zip"
$url = "https://github.com/libsdl-org/SDL/releases/download/release-$Version/SDL3-devel-$Version-mingw.zip"
$prefix = Join-Path $Destination "SDL3-$Version\x86_64-w64-mingw32"

if (Test-Path (Join-Path $prefix "include\SDL3\SDL.h")) {
    Write-Host "SDL3 $Version already installed at $prefix"
    exit 0
}

if (-not (Test-Path $zip)) {
    Write-Host "Downloading $url"
    Invoke-WebRequest -Uri $url -OutFile $zip -TimeoutSec 180
}

New-Item -ItemType Directory -Force $Destination | Out-Null
Expand-Archive -Path $zip -DestinationPath $Destination -Force
if (-not (Test-Path (Join-Path $prefix "include\SDL3\SDL.h"))) {
    throw "SDL3 extraction did not produce $prefix"
}
Write-Host "Installed SDL3 $Version at $prefix"
