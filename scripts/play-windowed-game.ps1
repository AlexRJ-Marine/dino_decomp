param(
    [string]$GameDir = "run-window",
    [switch]$NoClickCapture
)

# Launch the retail game in a normal bordered window. The mouse is NOT captured
# until you click inside the game window; Alt-Tab releases it again. This uses
# the same dgVoodoo2 windowed setup as the test harness, but a separate save
# directory so normal play does not disturb test reference saves.

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$wd = Join-Path $root $GameDir
$exe = Join-Path $wd "DinoIsland.exe"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DinoPlayFocus {
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int cmd);
}
"@
$previousForeground = [DinoPlayFocus]::GetForegroundWindow()

if (-not (Test-Path $exe)) {
    & (Join-Path $PSScriptRoot "setup-test-runtime.ps1") -Target $GameDir
}
if (-not (Test-Path $exe)) {
    throw "Windowed runtime not found: $exe"
}
if (-not (Test-Path (Join-Path $wd "DINPUT8.dll"))) {
    throw "DINPUT8.dll missing in $wd. Run scripts/build-hook.ps1 and setup again."
}

if (-not $NoClickCapture) {
    Remove-Item Env:DINO_VIRTUAL_MOUSE -ErrorAction SilentlyContinue
    $env:DINO_MOUSE_CLICK_CAPTURE = "1"
} else {
    Remove-Item Env:DINO_MOUSE_CLICK_CAPTURE -ErrorAction SilentlyContinue
}

$process = Start-Process -FilePath $exe -WorkingDirectory $wd -PassThru
Write-Host ("Dino Island windowed: pid={0}" -f $process.Id)

$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Milliseconds 500
    $process.Refresh()
    if ($process.HasExited) {
        throw "Dino Island exited early with code $($process.ExitCode)"
    }
    if ($process.MainWindowHandle -ne [IntPtr]::Zero -and
        $process.MainWindowTitle -like "*Dino*") {
        $hwnd = $process.MainWindowHandle
        break
    }
}
if ($hwnd -ne [IntPtr]::Zero) {
    & (Join-Path $PSScriptRoot "Set-WindowedStyle.ps1") -Hwnd $hwnd -SendToBack
}
if ($previousForeground -ne [IntPtr]::Zero) {
    [DinoPlayFocus]::SetForegroundWindow($previousForeground) | Out-Null
}
if ($hwnd -ne [IntPtr]::Zero) {
    # Focus restoration can deactivate the game; position it again without
    # activation so it stays visible in the background.
    Start-Sleep -Milliseconds 150
    & (Join-Path $PSScriptRoot "Set-WindowedStyle.ps1") -Hwnd $hwnd -SendToBack
}

Write-Host "Click inside the game window to give it mouse input; click outside to release it."
Write-Host "The game stays in the background; the physical cursor is never moved or clipped."
