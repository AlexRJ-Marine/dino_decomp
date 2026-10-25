param(
    [int]$Seconds = 120,
    [string]$GameDir = "run",
    [string]$Log = "notes\visitor-weights-runtime.out.log",
    [switch]$LoadSave
)

# Read-only recon:
#  - logs each .bvi temp record +0xD0 block as FUN_00544110 copies it
#  - logs the global weight vector and the five profile weights
#  - publishes the live weight pointer via savedata, then arms hardware read
#    breakpoints on profile weights 0 and 4 so their reader is recorded.
# No game files are modified.

$root = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
$h = "C:\Tools\x64dbg\release\x32\headless.exe"
$wd = Join-Path $root $GameDir
$exe = Join-Path $wd "DinoIsland.exe"
$logPath = Join-Path $root $Log
$dumpDir = "C:\Tools\dino_dumps"
$wptrFile = Join-Path $dumpDir "wptr.bin"

New-Item -ItemType Directory -Force $dumpDir | Out-Null
Remove-Item $wptrFile -ErrorAction SilentlyContinue
# Start from a clean breakpoint database; the headless debugger otherwise
# reloads breakpoints left by earlier sessions.
Remove-Item "C:\Tools\x64dbg\release\x32\headless\db\DinoIsland.exe.dd32" -ErrorAction SilentlyContinue

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
Send 'SetBreakpointCommand 0x51f560, "log GLOBAL_W_PTR={p:[esp+4]}_W0={x:[[esp+4]]}_W1={x:[[esp+4]+4]}_W2={x:[[esp+4]+8]}_W3={x:[[esp+4]+0xc]}_W4={x:[[esp+4]+0x10]}_W5={x:[[esp+4]+0x14]}_P0={x:[[esp+4]-0xac]}_P1={x:[[esp+4]-0xa8]}_P2={x:[[esp+4]-0xa4]}_P3={x:[[esp+4]-0xa0]}_P4={x:[[esp+4]-0x9c]}; savedata C:\Tools\dino_dumps\wptr.bin, esp+4, 4; run"'
for ($i = 0; $i -lt 8; $i++) {
    Send 'run'
    Start-Sleep -Milliseconds 800
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

if (-not $game) {
    Write-Host "game window not found"
} else {
    $hwnd = $game.MainWindowHandle
    Write-Host "game PID=$($game.Id) hwnd=$hwnd"
    if ($LoadSave) {
        Write-Host "clicking LOAD GAME"
        Click-Client $hwnd 400 280
        Start-Sleep -Seconds 2
        Write-Host "selecting first save"
        Click-Client $hwnd 395 150
        Start-Sleep -Seconds 1
        Write-Host "confirming load"
        Click-Client $hwnd 355 512
    } else {
        Click-Client $hwnd 395 234
        Start-Sleep -Seconds 2
        Click-Client $hwnd 260 183
        Start-Sleep -Seconds 1
        Click-Client $hwnd 494 451
        Start-Sleep -Seconds 1
        Click-Client $hwnd 526 531
    }
    Start-Sleep -Seconds 20

    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline -and -not (Test-Path $wptrFile)) {
        Start-Sleep -Milliseconds 300
    }
    if (Test-Path $wptrFile) {
        $bytes = [System.IO.File]::ReadAllBytes($wptrFile)
        $wptr = [BitConverter]::ToUInt32($bytes, 0)
        $pref0 = $wptr - 0xAC
        $pref4 = $wptr - 0x9C
        $h0 = '0x{0:X8}' -f $pref0
        $h4 = '0x{0:X8}' -f $pref4
        Write-Host "weight ptr=0x$('{0:X8}' -f $wptr) pref0=$h0 pref4=$h4"
        Send 'pause'
        Start-Sleep -Milliseconds 800
        Send ('bph {0}, r, 4' -f $h0)
        Send ('bph {0}, r, 4' -f $h4)
        Send ('SetHardwareBreakpointCommand {0}, "log PREF0_READ_cip={{p:cip}}; run"' -f $h0)
        Send ('SetHardwareBreakpointCommand {0}, "log PREF4_READ_cip={{p:cip}}; run"' -f $h4)
        Start-Sleep -Milliseconds 500
        Send 'run'
        Start-Sleep -Seconds 15
    } else {
        Write-Host "weight pointer file not written"
    }
}

Send 'exit'
if (-not $p.WaitForExit(15000)) { $p.Kill() }
$out = $outTask.Result
$err = $errTask.Result
$out | Set-Content -Path $logPath -Encoding utf8
$err | Set-Content -Path ($logPath -replace '\.out\.', '.err.') -Encoding utf8

Write-Host "--- matches ---"
Select-String -Path $logPath -Pattern "BVI_REC|GLOBAL_W|PREF0_READ|PREF4_READ|EXCEPTION|Invalid" | Select-Object -First 60 | ForEach-Object { $_.Line }
