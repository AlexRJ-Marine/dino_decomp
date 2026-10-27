# Shared tool locations. Dot-source from any script:
#     . (Join-Path $PSScriptRoot "tools.ps1")          # from scripts\
#     . (Join-Path $root "scripts\tools.ps1")          # from elsewhere
#
# Every external tool lives under one root, `$env:DINO_TOOLS` (default
# C:\Tools), so a contributor can put the toolchain anywhere without editing
# tracked files. `scripts/setup-tools.ps1` installs the pinned versions there.
# Individual overrides: SDL3_ROOT, JAVA_HOME (JDK 25), GHIDRA_INSTALL_DIR,
# DINO_X32DBG (path to x64dbg's x32 headless.exe).

$DinoTools = if ($env:DINO_TOOLS) { $env:DINO_TOOLS.TrimEnd('\', '/') } else { "C:\Tools" }

function Find-W64Devkit {
    # Returns @{ Gxx; Gcc; Bin } or $null.
    $base = Join-Path $DinoTools "w64devkit"
    $gxx = Get-ChildItem $base -Recurse -Filter "g++.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    $gcc = Get-ChildItem $base -Recurse -Filter "gcc.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $gxx -or -not $gcc) { return $null }
    @{ Gxx = $gxx.FullName; Gcc = $gcc.FullName; Bin = (Split-Path $gxx.FullName) }
}

function Find-Sdl3 {
    # Returns the x86_64-w64-mingw32 prefix or $null.
    $candidates = @()
    if ($env:SDL3_ROOT) { $candidates += $env:SDL3_ROOT }
    $candidates += Get-ChildItem (Join-Path $DinoTools "SDL3") -Directory -Filter "SDL3-*" -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | ForEach-Object { Join-Path $_.FullName "x86_64-w64-mingw32" }
    foreach ($prefix in $candidates) {
        if (Test-Path (Join-Path $prefix "include\SDL3\SDL.h")) { return $prefix }
    }
    $null
}

function Find-Jdk25 {
    # Returns the JDK 25 home or $null.
    # `java -version` writes to stderr; under a caller's "Stop" preference
    # Windows PowerShell 5.1 would turn that into a terminating error.
    $ErrorActionPreference = "Continue"
    $candidates = @()
    if ($env:JAVA_HOME) { $candidates += $env:JAVA_HOME }
    $candidates += Get-ChildItem (Join-Path $DinoTools "jdk25") -Directory -Filter "jdk-*" -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | ForEach-Object FullName
    foreach ($jdkHome in $candidates) {
        $java = Join-Path $jdkHome "bin\java.exe"
        if ((Test-Path $java) -and ((& $java -version 2>&1 | Select-Object -First 1) -match '"25\.')) { return $jdkHome }
    }
    $null
}

function Find-Ghidra {
    # Returns the Ghidra install dir or $null.
    if ($env:GHIDRA_INSTALL_DIR -and (Test-Path $env:GHIDRA_INSTALL_DIR)) { return $env:GHIDRA_INSTALL_DIR }
    $g = Get-ChildItem $DinoTools -Directory -Filter "ghidra_*_PUBLIC" -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | Select-Object -First 1
    if ($g) { $g.FullName } else { $null }
}

function Get-X32dbgHeadless {
    # Path to x64dbg's 32-bit headless debugger (may not exist yet).
    if ($env:DINO_X32DBG) { return $env:DINO_X32DBG }
    Join-Path $DinoTools "x64dbg\release\x32\headless.exe"
}

function Get-DinoDumpDir {
    # Scratch directory for x32dbg `savedata` dumps. Kept under the tools root
    # because x64dbg commands do not quote paths with spaces.
    $dir = Join-Path $DinoTools "dino_dumps"
    New-Item -ItemType Directory -Force $dir | Out-Null
    $dir
}
