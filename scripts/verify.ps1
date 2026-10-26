param(
    [switch]$Fast,
    [string]$BuildDir = "build\mingw",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

& (Join-Path $PSScriptRoot "check-env.ps1") -SkipTests
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Push-Location $root
try {
    $dirtyReadOnly = & git status --porcelain -- original game
    if ($LASTEXITCODE -ne 0) { throw "git status failed" }
    if ($dirtyReadOnly) {
        Write-Error "read-only tree changed:`n$dirtyReadOnly"
        exit 1
    }
    & git diff --check
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
    Pop-Location
}

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1") -BuildDir $BuildDir
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$exe = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
    Join-Path $BuildDir "dino_viewer.exe"
} else {
    Join-Path $root (Join-Path $BuildDir "dino_viewer.exe")
}
& (Join-Path $PSScriptRoot "run-tests.ps1") -Fast:$Fast -Exe $exe
exit $LASTEXITCODE
