param(
    [int]$Seconds = 75,
    [string]$GameDir = "run",
    [string]$Log = "notes\visitor-weights-runtime.out.log"
)

# Read-only recon: logs the .bvi profile record +0xD0 block as FUN_00544110
# copies each profile into the visitor list, and logs the global weight vector
# passed to FUN_0051f560. No game files are modified.

$root = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
$h = "C:\Tools\x64dbg\release\x32\headless.exe"
$wd = Join-Path $root $GameDir
$exe = Join-Path $wd "DinoIsland.exe"
$logPath = Join-Path $root $Log

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

Send 'bp 0x544110'
Send 'SetBreakpointCommand 0x544110, "log BVI_REC={p:[esp+4]}_0={x:[[esp+4]+0xd0]}_1={x:[[esp+4]+0xd4]}_2={x:[[esp+4]+0xd8]}_3={x:[[esp+4]+0xdc]}_4={x:[[esp+4]+0xe0]}_5={x:[[esp+4]+0xe4]}_6={x:[[esp+4]+0xe8]}; run"'
Send 'bp 0x51f560'
Send 'SetBreakpointCommand 0x51f560, "log GLOBAL_W_PTR={p:[esp+4]}_0={x:[[esp+4]]}_1={x:[[esp+4]+4]}_2={x:[[esp+4]+8]}_3={x:[[esp+4]+0xc]}_4={x:[[esp+4]+0x10]}_5={x:[[esp+4]+0x14]}; run"'
Send 'bp 0x522a10'
Send 'SetBreakpointCommand 0x522a10, "log SCORE_HIT={p:[0x675638]}; run"'
for ($i = 0; $i -lt 5; $i++) {
    Send 'run'
    Start-Sleep -Milliseconds 900
}

$deadline = (Get-Date).AddSeconds(25)
$game = $null
while ((Get-Date) -lt $deadline) {
    $game = Get-Process DinoIsland -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    if ($game) { break }
    Start-Sleep -Milliseconds 500
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
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

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

$game = Get-Process DinoIsland -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $game) {
    Write-Host "game process not found"
} else {
    $hwnd = $game.MainWindowHandle
    Write-Host "game PID=$($game.Id) hwnd=$hwnd"
    if ($hwnd -ne 0) {
        Write-Host "clicking Free Game"
        Click-Client $hwnd 395 234
        Start-Sleep -Seconds 2
        Write-Host "selecting IGUANO"
        Click-Client $hwnd 260 183
        Start-Sleep -Seconds 1
        Write-Host "selecting easy difficulty"
        Click-Client $hwnd 494 451
        Start-Sleep -Seconds 1
        Write-Host "clicking Start"
        Click-Client $hwnd 526 531
        Start-Sleep -Seconds 25
        Write-Host "park should be running"
    }
}

Start-Sleep -Seconds ([Math]::Max(2, $Seconds - 40))
Send 'exit'
if (-not $p.WaitForExit(15000)) { $p.Kill() }
$out = $outTask.Result
$err = $errTask.Result
$out | Set-Content -Path $logPath -Encoding utf8
$err | Set-Content -Path ($logPath -replace '\.out\.', '.err.') -Encoding utf8

Write-Host "--- matches ---"
Select-String -Path $logPath -Pattern "BVI_REC|GLOBAL_W|EXCEPTION" | ForEach-Object { $_.Line }
