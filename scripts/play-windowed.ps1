param(
    [int]$Seconds = 8,
    [string]$GameDir = "run-test",
    [string]$Shot = "",
    [switch]$Stop,
    [switch]$VirtualMouse
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$wd = Join-Path $root $GameDir
$exe = Join-Path $wd "DinoIsland.exe"
if (-not (Test-Path $exe)) {
    throw "Windowed runtime not found. Run scripts/setup-test-runtime.ps1 first."
}

if ($VirtualMouse) {
    $env:DINO_VIRTUAL_MOUSE = "1"
}

$process = Start-Process -FilePath $exe -WorkingDirectory $wd -PassThru
Start-Sleep -Seconds $Seconds

$game = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
if (-not $game) {
    throw "Dino Island exited early"
}
$hwnd = $game.MainWindowHandle
Write-Host ("Dino Island windowed: pid={0} hwnd=0x{1:X}" -f $game.Id, $hwnd.ToInt64())

if ($Shot) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DinoWindowShot {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
    Add-Type -AssemblyName System.Drawing
    $rect = New-Object DinoWindowShot+RECT
    [DinoWindowShot]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -gt 0 -and $height -gt 0) {
        $bmp = New-Object System.Drawing.Bitmap $width, $height
        $graphics = [System.Drawing.Graphics]::FromImage($bmp)
        try {
            $hdc = $graphics.GetHdc()
            [DinoWindowShot]::PrintWindow($hwnd, $hdc, 0) | Out-Null
            $graphics.ReleaseHdc($hdc)
        } finally {
            $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0,
                (New-Object System.Drawing.Size $width, $height))
            $graphics.Dispose()
        }
        $path = Join-Path $root $Shot
        $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        Write-Host "screenshot: $path"
    }
}

if ($Stop) {
    $game.CloseMainWindow() | Out-Null
    Start-Sleep -Milliseconds 800
    if (-not $game.HasExited) { $game.Kill() }
}
