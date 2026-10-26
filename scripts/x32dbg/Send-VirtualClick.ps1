param(
    [Parameter(Mandatory = $true)][int]$X,
    [Parameter(Mandatory = $true)][int]$Y,
    [string]$ProcessName = "DinoIsland",
    [string]$GameDir = "run-test",
    [int]$HoldMs = 100,
    [string]$Shot = "",
    [switch]$Focus
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
$gameDirPath = Join-Path $root $GameDir

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DinoVirtualInput {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int cmd);
  [DllImport("user32.dll", CharSet = CharSet.Ansi)] public static extern IntPtr FindWindowA(string className, string windowName);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint attach, uint attachTo, bool doAttach);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

$process = Get-Process $ProcessName -ErrorAction SilentlyContinue | Select-Object -First 1
$hwnd = if ($process) { $process.MainWindowHandle } else { [IntPtr]::Zero }
if ($hwnd -eq [IntPtr]::Zero) {
    $hwnd = [DinoVirtualInput]::FindWindowA("MCLib_Example", $null)
}
if ($hwnd -eq [IntPtr]::Zero) {
    throw "Dino Island window not found"
}

if ($Focus) {
    $foreground = [DinoVirtualInput]::GetForegroundWindow()
    $foregroundPid = 0
    $foregroundThread = [DinoVirtualInput]::GetWindowThreadProcessId($foreground, [ref]$foregroundPid)
    $currentThread = [DinoVirtualInput]::GetCurrentThreadId()
    [DinoVirtualInput]::AttachThreadInput($currentThread, $foregroundThread, $true) | Out-Null
    [DinoVirtualInput]::ShowWindow($hwnd, 9) | Out-Null
    [DinoVirtualInput]::BringWindowToTop($hwnd) | Out-Null
    [DinoVirtualInput]::SetForegroundWindow($hwnd) | Out-Null
    [DinoVirtualInput]::AttachThreadInput($currentThread, $foregroundThread, $false) | Out-Null
    Start-Sleep -Milliseconds 300
} else {
    # Restore without activation; the game stays behind other windows.
    [DinoVirtualInput]::ShowWindow($hwnd, 4) | Out-Null
}

# WM_MOUSEMOVE updates the game's own cursor; the DirectInput proxy supplies
# the button state from dino_input.txt. The physical cursor is never moved.
$lParam = [IntPtr](($Y -shl 16) -bor ($X -band 0xffff))
[DinoVirtualInput]::PostMessage($hwnd, 0x200, [IntPtr]::Zero, $lParam) | Out-Null
Start-Sleep -Milliseconds 80

$inputPath = Join-Path $gameDirPath "dino_input.txt"
Set-Content -Path $inputPath -Value ("{0} {1} 1" -f $X, $Y) -NoNewline
# Also post the window messages; the main WndProc ignores them, but another
# handler may consume them in some builds.
[DinoVirtualInput]::PostMessage($hwnd, 0x201, [IntPtr]1, $lParam) | Out-Null
Start-Sleep -Milliseconds $HoldMs
[DinoVirtualInput]::PostMessage($hwnd, 0x202, [IntPtr]0, $lParam) | Out-Null
Set-Content -Path $inputPath -Value ("{0} {1} 0" -f $X, $Y) -NoNewline
Start-Sleep -Milliseconds 80

if ($Shot) {
    Add-Type -AssemblyName System.Drawing
    $rect = New-Object DinoVirtualInput+RECT
    [DinoVirtualInput]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    $bmp = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $graphics.GetHdc()
    [DinoVirtualInput]::PrintWindow($hwnd, $hdc, 0) | Out-Null
    $graphics.ReleaseHdc($hdc)
    $graphics.Dispose()
    $bmp.Save($Shot, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

Write-Host ("virtual click {0},{1} -> hwnd 0x{2:X}" -f $X, $Y, $hwnd.ToInt64())
