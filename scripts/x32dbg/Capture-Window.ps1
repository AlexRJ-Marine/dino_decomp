param(
    [Parameter(Mandatory = $true)][string]$Path,
    [string]$ProcessName = "DinoIsland"
)

$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DinoCapture {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
Add-Type -AssemblyName System.Drawing

$game = Get-Process $ProcessName -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $game) { throw "process not found: $ProcessName" }
$hwnd = $game.MainWindowHandle
if ($hwnd -eq [IntPtr]::Zero) { throw "no main window" }

# Restore without activation if minimized; never raise the game window.
[DinoCapture]::ShowWindow($hwnd, 4) | Out-Null
Start-Sleep -Milliseconds 200

$rect = New-Object DinoCapture+RECT
[DinoCapture]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
$bmp = New-Object System.Drawing.Bitmap $width, $height
$graphics = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $graphics.GetHdc()
[DinoCapture]::PrintWindow($hwnd, $hdc, 0) | Out-Null
$graphics.ReleaseHdc($hdc)
$graphics.Dispose()
$bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "captured $Path"
