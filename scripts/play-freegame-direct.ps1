param(
    [int]$Seconds = 45,
    [string]$GameDir = "run",
    [string]$Shot = "notes\park-hook.png"
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$wd = Join-Path $root $GameDir
$exe = Join-Path $wd "DinoIsland.exe"
if (-not (Test-Path $exe)) { throw "not found: $exe" }

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32 {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
Add-Type -AssemblyName System.Drawing

function Save-Window([IntPtr]$hwnd, [string]$path) {
    $r = New-Object Win32+RECT
    [Win32]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $w = $r.Right - $r.Left
    $h = $r.Bottom - $r.Top
    if ($w -le 0 -or $h -le 0) { return }
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    try {
        $hdc = $g.GetHdc()
        [Win32]::PrintWindow($hwnd, $hdc, 0) | Out-Null
        $g.ReleaseHdc($hdc)
    } finally {
        $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size $w, $h))
        $g.Dispose()
    }
    $bmp.Save((Join-Path $root $path), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function Click-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    [Win32]::ShowWindow($hwnd, 9) | Out-Null
    [Win32]::SetForegroundWindow($hwnd) | Out-Null
    Start-Sleep -Milliseconds 250
    $r = New-Object Win32+RECT
    [Win32]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    [Win32]::SetCursorPos($r.Left + $x, $r.Top + $y) | Out-Null
    Start-Sleep -Milliseconds 120
    [Win32]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Win32]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 250
}

$p = Start-Process -FilePath $exe -WorkingDirectory $wd -PassThru
$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 30; $i++) {
    Start-Sleep -Milliseconds 500
    $p.Refresh()
    if ($p.HasExited) { throw "game exited early with $($p.ExitCode)" }
    if ($p.MainWindowHandle -ne [IntPtr]::Zero) {
        $hwnd = $p.MainWindowHandle
        break
    }
}
if ($hwnd -eq [IntPtr]::Zero) { throw "no game window" }
Write-Host "game pid=$($p.Id) hwnd=$hwnd title='$($p.MainWindowTitle)'"

Start-Sleep -Seconds 3
Click-Client $hwnd 395 234
Start-Sleep -Seconds 2
Click-Client $hwnd 260 183
Start-Sleep -Seconds 1
Click-Client $hwnd 494 451
Start-Sleep -Seconds 1
Click-Client $hwnd 526 531
Start-Sleep -Seconds 20
Save-Window $hwnd $Shot
Write-Host "saved $Shot"

Start-Sleep -Seconds ([Math]::Max(2, $Seconds - 27))
if (-not $p.HasExited) {
    $p.CloseMainWindow() | Out-Null
    if (-not $p.WaitForExit(5000)) { $p.Kill() }
}
Write-Host "--- hook log ---"
$log = Join-Path $wd "dino_hook.log"
if (Test-Path $log) {
    Get-Content $log
} else {
    Write-Host "no hook log"
}
