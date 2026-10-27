param(
    [int]$Seconds = 75,
    [int]$IslandRow = 0,
    [string]$GameDir = "",
    [string]$Log = "notes\x32dbg-calibration.out.log",
    [switch]$PhysicalInput
)

# Runtime calibration for:
#   - water-surface z        FUN_00602cf0 [ecx+0x30]
#   - free-game Entrance     FUN_00586b80 [[esp+4]], [[esp+4]+4], [esp+8]
#   - visitor needs/profile  FUN_00576320 / FUN_005763f0 (ecx+0x28/48/4c/50/68/74)
#   - visitor spawn queue    FUN_0058a970 (visitor count / profile table)
#
# The game is started through the retail menu with the selected island row.

$root = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
. (Join-Path $root "scripts\tools.ps1")
$h = Get-X32dbgHeadless
if (-not $GameDir) {
    $GameDir = if ($PhysicalInput) { "run" } else { "run-test" }
}
$wd = Join-Path $root $GameDir
$exe = Join-Path $wd "DinoIsland.exe"
$logPath = Join-Path $root $Log

$previousForeground = [IntPtr]::Zero
if (-not $PhysicalInput) {
    if (-not (Test-Path $exe)) {
        & (Join-Path $root "scripts\setup-test-runtime.ps1")
    }
    $env:DINO_VIRTUAL_MOUSE = "1"
    . (Join-Path $PSScriptRoot "VirtualInput.ps1")
    $previousForeground = [DinoVirtualInputShared]::GetForegroundWindow()
}

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

# Clear inherited breakpoints from the global x32dbg config (the CreateFile
# breakpoints left by earlier sessions stop headless without a command).
Send 'bc'

# Water surface: FUN_00602cf0 builds the quad with z at water+0x30.
Send 'bp 0x602cf0'
Send 'SetBreakpointCommand 0x602cf0, "log WATER_Z={p:[ecx+0x30]} W={p:[ecx+0x28]} H={p:[ecx+0x2c]}; run"'

# Building ctor: param_2 = cell pair, param_3 = orientation.
Send 'bp 0x586b80'
Send 'SetBreakpointCommand 0x586b80, "log BUILD ret={p:[esp]} cellx={p:[[esp+4]]} celly={p:[[esp+4]+4]} state={p:[esp+8]}; run"'

# Visitor timed need update.
Send 'bp 0x576320'
Send 'SetBreakpointCommand 0x576320, "log VNEED ecx={p:ecx} st={p:[ecx+0x28]} n0={p:[ecx+0x48]} n1={p:[ecx+0x4c]} n2={p:[ecx+0x50]} prof={p:[ecx+0x68]} leave={p:[ecx+0x74]}; run"'
Send 'bp 0x5763f0'
Send 'SetBreakpointCommand 0x5763f0, "log VNEED0 ecx={p:ecx} st={p:[ecx+0x28]} n0={p:[ecx+0x48]} n1={p:[ecx+0x4c]} n2={p:[ecx+0x50]} prof={p:[ecx+0x68]} leave={p:[ecx+0x74]}; run"'

# Spawn selection: visitor vector count and profile count.
Send 'bp 0x58a970'
Send 'SetBreakpointCommand 0x58a970, "log VSPAWN ecx={p:ecx} vbeg={p:[ecx+0x80]} vend={p:[ecx+0x84]} cap={p:[ecx+0x18]} prof={p:[ecx+0x78]}; run"'

Send 'run'
Start-Sleep -Milliseconds 700
Send 'run'
Start-Sleep -Milliseconds 700
Send 'run'

Start-Sleep -Seconds 8

# The dinput8 TLS callbacks can stop the debugger several times before the
# window appears; keep resuming until the game is running.
for ($i = 0; $i -lt 15; $i++) {
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
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
Add-Type -AssemblyName Microsoft.VisualBasic
Add-Type -AssemblyName System.Windows.Forms

function Click-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    if (-not $PhysicalInput) {
        Invoke-VirtualClick -Hwnd $hwnd -X $x -Y $y -GameDir $wd
        return
    }
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

$game = Get-Process DinoIsland -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $game) {
    Write-Host "game process not found"
} else {
    $hwnd = $game.MainWindowHandle
if (-not $PhysicalInput -and $hwnd -ne [IntPtr]::Zero) {
    & (Join-Path $root "scripts\Set-WindowedStyle.ps1") -Hwnd $hwnd -SendToBack
    Restore-Foreground $previousForeground
}
    if ($hwnd -eq 0) {
        Write-Host "no main window handle"
    } else {
        [Microsoft.VisualBasic.Interaction]::AppActivate($game.Id) | Out-Null
        Write-Host "clicking Free Game"
        Click-Client $hwnd 395 234
        Start-Sleep -Seconds 2
        Write-Host "selecting island row $IslandRow"
        $islandY = 166 + $IslandRow * 28 + 14
        Click-Client $hwnd 260 $islandY
        Start-Sleep -Seconds 1
        Write-Host "selecting easy difficulty"
        Click-Client $hwnd 494 451
        Start-Sleep -Seconds 1
        Write-Host "clicking Start"
        Click-Client $hwnd 526 531
        Start-Sleep -Seconds 35
        Write-Host "waiting for visitors/decay"
    }
}

Start-Sleep -Seconds ([Math]::Max(2, $Seconds - 48))
Send 'exit'
if (-not $p.WaitForExit(10000)) { $p.Kill() }
$out = $outTask.Result
$err = $errTask.Result
$out | Set-Content -Path $logPath -Encoding utf8
$err | Set-Content -Path ($logPath -replace '\.out\.', '.err.') -Encoding utf8

Write-Host "--- calibration matches ---"
Select-String -Path $logPath -Pattern "WATER_Z|BUILD |VNEED|VSPAWN|EXCEPTION" | Select-Object -First 200
