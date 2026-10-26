# Shared virtual-input helpers for the x32dbg harness.
#
# The DINPUT8 proxy reads run-test\dino_input.txt and feeds the mouse buttons
# through DirectInput. WM_MOUSEMOVE updates the game's cursor. No physical
# cursor movement or mouse_event call is used.

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DinoVirtualInputShared {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
}
"@

function Set-GameForeground([IntPtr]$Hwnd) {
    $foreground = [DinoVirtualInputShared]::GetForegroundWindow()
    $foregroundPid = 0
    $foregroundThread = [DinoVirtualInputShared]::GetWindowThreadProcessId(
        $foreground, [ref]$foregroundPid)
    $currentThread = [DinoVirtualInputShared]::GetCurrentThreadId()
    [DinoVirtualInputShared]::AttachThreadInput(
        $currentThread, $foregroundThread, $true) | Out-Null
    [DinoVirtualInputShared]::ShowWindow($Hwnd, 9) | Out-Null
    [DinoVirtualInputShared]::BringWindowToTop($Hwnd) | Out-Null
    [DinoVirtualInputShared]::SetForegroundWindow($Hwnd) | Out-Null
    [DinoVirtualInputShared]::AttachThreadInput(
        $currentThread, $foregroundThread, $false) | Out-Null
    Start-Sleep -Milliseconds 200
}

function Invoke-VirtualClick(
    [IntPtr]$Hwnd,
    [int]$X,
    [int]$Y,
    [string]$GameDir,
    [int]$HoldMs = 100
) {
    Set-GameForeground $Hwnd
    $lParam = [IntPtr](($Y -shl 16) -bor ($X -band 0xffff))
    [DinoVirtualInputShared]::PostMessage(
        $Hwnd, 0x200, [IntPtr]::Zero, $lParam) | Out-Null
    Start-Sleep -Milliseconds 80

    $inputPath = Join-Path $GameDir "dino_input.txt"
    Set-Content -Path $inputPath -Value ("{0} {1} 1" -f $X, $Y) -NoNewline
    [DinoVirtualInputShared]::PostMessage(
        $Hwnd, 0x201, [IntPtr]1, $lParam) | Out-Null
    Start-Sleep -Milliseconds $HoldMs
    [DinoVirtualInputShared]::PostMessage(
        $Hwnd, 0x202, [IntPtr]0, $lParam) | Out-Null
    Set-Content -Path $inputPath -Value ("{0} {1} 0" -f $X, $Y) -NoNewline
    Start-Sleep -Milliseconds 100
}
