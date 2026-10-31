# Launch the portable Dino Island viewer for interactive play.
#
# Default: the main menu (Campaign / Free Game). Menu-started games enable the
# live dino action loop and use the retail park camera.
# -Sandbox: load the Cpg04 park directly (no menu) with the retail camera and
# the action loop, for testing construction/feeding.
#
# This script only starts scripts/play-windowed-game.ps1's equivalent for the
# port; it does not touch game/ or original/.
param(
    [string]$BuildDir = "build/lunchui",
    [switch]$Sandbox,
    [int]$Width = 1280,
    [int]$Height = 800,
    [string]$AssetRoot = "game/%MAINDIR%"
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root (Join-Path $BuildDir "dino_port.exe")
if (-not (Test-Path $exe)) {
    throw "port not built: $exe (run scripts/build.ps1 -BuildDir $BuildDir)"
}
$asset = (Resolve-Path (Join-Path $root $AssetRoot)).Path
$portArgs = @("--width", "$Width", "--height", "$Height", "--asset-root", $asset)
if ($Sandbox) {
    $lnd = Join-Path $asset "Data/Design/landscape/isl04/isl04.lnd"
    $wld = Join-Path $asset "Data/Design/Scenario/Cpg04/Cpg04_Save.wld"
    $portArgs += @("--no-menu", "--lnd", $lnd, "--wld-park", $wld,
                   "--camera-retail", "--dino-action-loop",
                   "--park-budget", "75000")
}
$argLine = ($portArgs | ForEach-Object {
    if ($_ -match '\s') { '"' + $_ + '"' } else { $_ }
}) -join ' '
$p = Start-Process -FilePath $exe -ArgumentList $argLine -WorkingDirectory $root -PassThru
Write-Host ("port: pid={0} mode={1} exe={2}" -f $p.Id,
            $(if ($Sandbox) { "Cpg04 sandbox" } else { "menu" }), $exe)
Write-Host "Close the window (or stop the pid) when done."
