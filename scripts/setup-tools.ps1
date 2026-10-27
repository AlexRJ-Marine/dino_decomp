# One-command toolchain setup for a new machine. Idempotent: anything already
# present is left alone, so re-running it is safe.
#
#   .\scripts\setup-tools.ps1                 # everything, under $env:DINO_TOOLS (default C:\Tools)
#   .\scripts\setup-tools.ps1 -Only venv,w64devkit,sdl3     # just what the port build needs
#   $env:DINO_TOOLS = "D:\dev\tools"; .\scripts\setup-tools.ps1
#
# Versions are pinned so every contributor builds with the same toolchain.
# Bump them here (and nowhere else) when the project moves to a new release.
param(
    [ValidateSet("venv", "w64devkit", "sdl3", "jdk", "ghidra", "x64dbg")]
    [string[]]$Only = @("venv", "w64devkit", "sdl3", "jdk", "ghidra", "x64dbg")
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot "tools.ps1")

$pins = @{
    w64devkit = @{ Version = "2.10.1"
        Url = "https://github.com/skeeto/w64devkit/releases/download/v2.10.1/w64devkit-x64-2.10.1.7z.exe" }
    sdl3      = @{ Version = "3.4.18" }
    jdk       = @{ Version = "25.0.5+8"
        Url = "https://github.com/adoptium/temurin25-binaries/releases/download/jdk-25.0.5%2B8/OpenJDK25U-jdk_x64_windows_hotspot_25.0.5_8.zip" }
    ghidra    = @{ Version = "12.2"
        Url = "https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.2_build/ghidra_12.2_PUBLIC_20261016.zip" }
    x64dbg    = @{ Version = "2026.05.27"
        Url = "https://github.com/x64dbg/x64dbg/releases/download/2026.05.27/snapshot_2026-05-27_12-11.zip" }
}

$download = Join-Path $env:TEMP "dino-tools-download"
New-Item -ItemType Directory -Force $DinoTools, $download | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-Download($url) {
    $file = Join-Path $download ([Uri]::UnescapeDataString(($url -split '/')[-1]))
    if (-not (Test-Path $file)) {
        Write-Host "  downloading $url"
        Invoke-WebRequest -Uri $url -OutFile "$file.part" -TimeoutSec 1800
        Move-Item "$file.part" $file -Force
    }
    $file
}

function Step($name, $present, $detail, [scriptblock]$install) {
    if (-not ($Only -contains $name)) { return }
    if (& $present) {
        Write-Host ("[ OK ] {0,-10} already installed: {1}" -f $name, (& $detail))
        return
    }
    Write-Host ("[....] {0,-10} installing" -f $name)
    & $install
    if (-not (& $present)) { throw "$name install did not produce the expected files" }
    Write-Host ("[ OK ] {0,-10} installed: {1}" -f $name, (& $detail))
}

Write-Host "Tools root: $DinoTools"

Step "venv" { Test-Path (Join-Path $root ".venv\Scripts\cmake.exe") } { Join-Path $root ".venv" } {
    $py = Get-Command py -ErrorAction SilentlyContinue
    $pyArgs = if ($py) { @("-3") } else { @() }
    if (-not $py) { $py = Get-Command python -ErrorAction SilentlyContinue }
    if (-not $py -or $py.Source -like "*WindowsApps*") {
        throw "Python 3.11+ is required. Install it from python.org (tick 'Add to PATH'), then re-run."
    }
    & $py.Source @pyArgs -m venv (Join-Path $root ".venv")
    $vpy = Join-Path $root ".venv\Scripts\python.exe"
    & $vpy -m pip install --upgrade pip -q
    & $vpy -m pip install -r (Join-Path $root "requirements.txt") -q
}

Step "w64devkit" { $null -ne (Find-W64Devkit) } { (Find-W64Devkit).Gxx } {
    $sfx = Get-Download $pins.w64devkit.Url
    $dest = Join-Path $DinoTools "w64devkit"
    New-Item -ItemType Directory -Force $dest | Out-Null
    $p = Start-Process -FilePath $sfx -ArgumentList "-o$dest", "-y" -Wait -PassThru -WindowStyle Hidden
    if ($p.ExitCode -ne 0) { throw "w64devkit self-extractor exited with $($p.ExitCode)" }
}

Step "sdl3" { $null -ne (Find-Sdl3) } { Find-Sdl3 } {
    & (Join-Path $PSScriptRoot "setup-sdl3.ps1") -Version $pins.sdl3.Version
}

Step "jdk" { $null -ne (Find-Jdk25) } { Find-Jdk25 } {
    $zip = Get-Download $pins.jdk.Url
    $dest = Join-Path $DinoTools "jdk25"
    New-Item -ItemType Directory -Force $dest | Out-Null
    [System.IO.Compression.ZipFile]::ExtractToDirectory($zip, $dest)
}

Step "ghidra" { $null -ne (Find-Ghidra) } { Find-Ghidra } {
    $zip = Get-Download $pins.ghidra.Url
    [System.IO.Compression.ZipFile]::ExtractToDirectory($zip, $DinoTools)
}

Step "x64dbg" { Test-Path (Get-X32dbgHeadless) } { Get-X32dbgHeadless } {
    $zip = Get-Download $pins.x64dbg.Url
    $dest = Join-Path $DinoTools "x64dbg"
    New-Item -ItemType Directory -Force $dest | Out-Null
    [System.IO.Compression.ZipFile]::ExtractToDirectory($zip, $dest)
}

Write-Host ""
Write-Host "Done. Next: .\scripts\build.ps1 -BuildDir build\<lane>, then .\scripts\check-env.ps1"
if ($env:DINO_TOOLS) {
    Write-Host "DINO_TOOLS is set for this shell only. To keep it, run:"
    Write-Host "  [Environment]::SetEnvironmentVariable('DINO_TOOLS', '$DinoTools', 'User')"
}
exit 0
