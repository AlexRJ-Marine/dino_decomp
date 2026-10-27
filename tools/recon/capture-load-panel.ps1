param(
    [string]$OutFile = "notes\load-panel.png"
)

# Read-only UI capture: open the LOAD GAME panel and screenshot it.

$root = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
. (Join-Path $root "scripts\tools.ps1")
$h = Get-X32dbgHeadless
$wd = Join-Path $root "run"
$exe = Join-Path $wd "DinoIsland.exe"

Remove-Item (Join-Path (Split-Path $h) "headless\db\DinoIsland.exe.dd32") -ErrorAction SilentlyContinue

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $h
$psi.Arguments = "-workingDir `"$wd`" `"$exe`""
$psi.UseShellExecute = $false
$psi.RedirectStandardInput = $true
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.CreateNoWindow = $true
$psi.WorkingDirectory = $root

$p = [System.Diagnostics.Process]::Start($psi)
$outTask = $p.StandardOutput.ReadToEndAsync()
$errTask = $p.StandardError.ReadToEndAsync()

function Send([string]$line) {
    $p.StandardInput.WriteLine($line)
    $p.StandardInput.Flush()
}

for ($i = 0; $i -lt 8; $i++) {
    Send 'run'
    Start-Sleep -Milliseconds 800
}

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
Add-Type -AssemblyName Microsoft.VisualBasic
Add-Type -AssemblyName System.Windows.Forms

function Save-Window([IntPtr]$hwnd, [string]$name) {
    $r = New-Object Win32+RECT
    [Win32]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $w = $r.Right - $r.Left
    $hgt = $r.Bottom - $r.Top
    if ($w -le 0 -or $hgt -le 0) { return }
    $bmp = New-Object System.Drawing.Bitmap $w, $hgt
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    try {
        $hdc = $g.GetHdc()
        [Win32]::PrintWindow($hwnd, $hdc, 0) | Out-Null
        $g.ReleaseHdc($hdc)
    } finally {
        $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size $w, $hgt))
        $g.Dispose()
    }
    $bmp.Save((Join-Path $root $name), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

function Click-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    [Win32]::ShowWindow($hwnd, 9) | Out-Null
    [Win32]::SetForegroundWindow($hwnd) | Out-Null
    Start-Sleep -Milliseconds 300
    $r = New-Object Win32+RECT
    [Win32]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    [Win32]::SetCursorPos($r.Left + $x, $r.Top + $y) | Out-Null
    Start-Sleep -Milliseconds 150
    [Win32]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Win32]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 300
}

$deadline = (Get-Date).AddSeconds(25)
$game = $null
while ((Get-Date) -lt $deadline) {
    $game = Get-Process DinoIsland -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    if ($game) { break }
    Start-Sleep -Milliseconds 500
}

if ($game) {
    $hwnd = $game.MainWindowHandle
    Write-Host "game PID=$($game.Id) hwnd=$hwnd"
    [Microsoft.VisualBasic.Interaction]::AppActivate($game.Id) | Out-Null
    Save-Window $hwnd ($OutFile -replace '\.png$', '-menu.png')
    Write-Host "clicking LOAD GAME"
    Click-Client $hwnd 400 280
    Start-Sleep -Seconds 3
    Save-Window $hwnd $OutFile
} else {
    Write-Host "game window not found"
}

Start-Sleep -Seconds 2
Send 'exit'
if (-not $p.WaitForExit(10000)) { $p.Kill() }
$outTask.Result | Out-Null
$errTask.Result | Out-Null
