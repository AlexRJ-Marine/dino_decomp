param(
    [Parameter(Mandatory = $true)][IntPtr]$Hwnd,
    [switch]$SendToBack
)

# The retail app creates a WS_POPUP window (no title bar or border). dgVoodoo2
# keeps that style in windowed mode. Give it a normal overlapped window while
# preserving the 800x600 client area and the game's rendering.
$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DinoWindowStyle {
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern int GetWindowLongA(IntPtr h, int index);
  [DllImport("user32.dll")] public static extern int SetWindowLongA(IntPtr h, int index, int value);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool AdjustWindowRectEx(ref RECT r, int style, bool menu, int exStyle);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

if (-not [DinoWindowStyle]::IsWindow($Hwnd)) {
    throw "invalid game window"
}
# SW_SHOWNOACTIVATE: restore a minimized window without taking focus.
[DinoWindowStyle]::ShowWindow($Hwnd, 4) | Out-Null
Start-Sleep -Milliseconds 100

$GWL_STYLE = -16
# WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION |
# WS_SYSMENU | WS_MINIMIZEBOX
$style = 0x16CA0000
$old = [DinoWindowStyle]::GetWindowLongA($Hwnd, $GWL_STYLE)
[DinoWindowStyle]::SetWindowLongA($Hwnd, $GWL_STYLE, $style) | Out-Null

$current = New-Object DinoWindowStyle+RECT
[DinoWindowStyle]::GetWindowRect($Hwnd, [ref]$current) | Out-Null
$client = New-Object DinoWindowStyle+RECT
$client.Right = 800
$client.Bottom = 600
[DinoWindowStyle]::AdjustWindowRectEx([ref]$client, $style, $false, 0) | Out-Null
$width = $client.Right - $client.Left
$height = $client.Bottom - $client.Top
$centerX = [int](($current.Left + $current.Right) / 2)
$centerY = [int](($current.Top + $current.Bottom) / 2)
if ($current.Left -lt -10000 -or ($current.Right - $current.Left) -le 0) {
    $centerX = [int]([DinoWindowStyle]::GetSystemMetrics(0) / 2)
    $centerY = [int]([DinoWindowStyle]::GetSystemMetrics(1) / 2)
}
$x = $centerX - [int]($width / 2)
$y = $centerY - [int]($height / 2)

# SWP_FRAMECHANGED | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOOWNERZORDER |
# SWP_NOACTIVATE
$flags = 0x0020 -bor 0x0004 -bor 0x0040 -bor 0x0200 -bor 0x0010
[DinoWindowStyle]::SetWindowPos($Hwnd, [IntPtr]::Zero, $x, $y, $width, $height, $flags) | Out-Null

# The app/dgVoodoo may adjust the window after the style change. Measure the
# client area and correct it to 800x600.
for ($i = 0; $i -lt 3; $i++) {
    Start-Sleep -Milliseconds 100
    $windowRect = New-Object DinoWindowStyle+RECT
    $clientRect = New-Object DinoWindowStyle+RECT
    [DinoWindowStyle]::GetWindowRect($Hwnd, [ref]$windowRect) | Out-Null
    [DinoWindowStyle]::GetClientRect($Hwnd, [ref]$clientRect) | Out-Null
    $clientWidth = $clientRect.Right - $clientRect.Left
    $clientHeight = $clientRect.Bottom - $clientRect.Top
    $deltaWidth = 800 - $clientWidth
    $deltaHeight = 600 - $clientHeight
    if ($deltaWidth -eq 0 -and $deltaHeight -eq 0) {
        break
    }
    $newWidth = ($windowRect.Right - $windowRect.Left) + $deltaWidth
    $newHeight = ($windowRect.Bottom - $windowRect.Top) + $deltaHeight
    # SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED |
    # SWP_NOACTIVATE
    [DinoWindowStyle]::SetWindowPos(
        $Hwnd, [IntPtr]::Zero, 0, 0, $newWidth, $newHeight,
        0x0002 -bor 0x0004 -bor 0x0200 -bor 0x0020 -bor 0x0010) | Out-Null
}

if ($SendToBack) {
    # HWND_BOTTOM, no move/size/activate: keep the game behind the user's work.
    [DinoWindowStyle]::SetWindowPos(
        $Hwnd, [IntPtr]1, 0, 0, 0, 0,
        0x0001 -bor 0x0002 -bor 0x0010) | Out-Null
}

Write-Host ("window style: 0x{0:X8} -> 0x{1:X8}; client 800x600" -f $old, $style)
