param(
    [int]$Seconds = 55,
    [string]$GameDir = "",
    [string]$Log = "notes\x32dbg-freegame.out.log",
    [string]$ShotDir = "notes\harness",
    [switch]$PhysicalInput
)

$root = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
$h = "C:\Tools\x64dbg\release\x32\headless.exe"
if (-not $GameDir) {
    $GameDir = if ($PhysicalInput) { "run" } else { "run-test" }
}
$wd = Join-Path $root $GameDir
$exe = Join-Path $wd "DinoIsland.exe"
$logPath = Join-Path $root $Log

if (-not $PhysicalInput) {
    if (-not (Test-Path $exe)) {
        & (Join-Path $root "scripts\setup-test-runtime.ps1")
    }
    $env:DINO_VIRTUAL_MOUSE = "1"
    . (Join-Path $PSScriptRoot "VirtualInput.ps1")
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

# File opens and format loaders.
Send 'bp CreateFileA'
Send 'SetBreakpointCommand CreateFileA, "log CF_{s:[esp+4]}; run"'
Send 'bp CreateFileW'
Send 'SetBreakpointCommand CreateFileW, "log CW_{s:[esp+4]}; run"'
Send 'bp 0x48b870'
Send 'SetBreakpointCommand 0x48b870, "log SKIN_LOADER_ecx={p:ecx}_a={p:[esp+4]}_b={p:[esp+8]}; run"'
Send 'bp 0x475e30'
Send 'SetBreakpointCommand 0x475e30, "log SKEL_LOADER_ecx={p:ecx}; run"'
Send 'bp 0x48b800'
Send 'SetBreakpointCommand 0x48b800, "log MESH_LOADER_ecx={p:ecx}_a={p:[esp+4]}_b={p:[esp+8]}; run"'
New-Item -ItemType Directory -Force C:\Tools\dino_dumps | Out-Null
Send 'bp 0x402bc0'
Send 'SetBreakpointCommand 0x402bc0, "log SAVE_CALLED_ecx={p:ecx}; run"'
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
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName Microsoft.VisualBasic
Add-Type -AssemblyName System.Windows.Forms

function Save-Window([IntPtr]$hwnd, [string]$name) {
    if (-not $PhysicalInput) {
        Set-GameForeground $hwnd
    } else {
        [Win32]::ShowWindow($hwnd, 9) | Out-Null
        [Win32]::SetForegroundWindow($hwnd) | Out-Null
        Start-Sleep -Milliseconds 200
    }
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
        # fall back to screen if PrintWindow produced a blank image
        $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size $w, $hgt))
        $g.Dispose()
    }
    $dir = Join-Path $root $ShotDir
    New-Item -ItemType Directory -Force $dir | Out-Null
    $bmp.Save((Join-Path $dir "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

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
    [Win32]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero) # left down
    Start-Sleep -Milliseconds 80
    [Win32]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero) # left up
    Start-Sleep -Milliseconds 300
}

$game = Get-Process DinoIsland -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $game) {
    Write-Host "game process not found"
} else {
    Write-Host "game PID=$($game.Id) title='$($game.MainWindowTitle)' hwnd=$($game.MainWindowHandle)"
    $hwnd = $game.MainWindowHandle
if (-not $PhysicalInput -and $hwnd -ne [IntPtr]::Zero) {
    & (Join-Path $root "scripts\Set-WindowedStyle.ps1") -Hwnd $hwnd
}
    if ($hwnd -eq 0) {
        Write-Host "no main window handle"
    } else {
        [Microsoft.VisualBasic.Interaction]::AppActivate($game.Id) | Out-Null
        Save-Window $hwnd "menu-main"
        Write-Host "clicking Free Game"
        Click-Client $hwnd 395 234
        Start-Sleep -Seconds 2
        Save-Window $hwnd "menu-freegame"
        Write-Host "selecting IGUANO"
        Click-Client $hwnd 260 183
        Start-Sleep -Seconds 1
        Write-Host "selecting easy difficulty"
        Click-Client $hwnd 494 451
        Start-Sleep -Seconds 1
        Save-Window $hwnd "menu-freegame-selected"
        Write-Host "clicking Start"
        Click-Client $hwnd 526 531
        Start-Sleep -Seconds 15
        Save-Window $hwnd "park-loading2"
        Start-Sleep -Seconds 10
        Save-Window $hwnd "park-after2"
        Write-Host "opening lab panel"
        Click-Client $hwnd 757 46
        Start-Sleep -Seconds 1
        Save-Window $hwnd "lab-menu"
        Write-Host "opening options panel"
        Click-Client $hwnd 765 348
        Start-Sleep -Seconds 1
        Save-Window $hwnd "options-panel"
        Write-Host "clicking save icon"
        Click-Client $hwnd 340 72
        Start-Sleep -Seconds 1
        Save-Window $hwnd "save-panel"
        Write-Host "typing save name"
        Click-Client $hwnd 349 382
        Start-Sleep -Milliseconds 300
        [System.Windows.Forms.SendKeys]::SendWait("agenttest")
        Start-Sleep -Milliseconds 500
        Save-Window $hwnd "save-panel-named"
        Write-Host "clicking green check"
        Click-Client $hwnd 334 436
        Start-Sleep -Seconds 2
        Save-Window $hwnd "save-after"
        Write-Host "switching to Load tab"
        Click-Client $hwnd 370 72
        Start-Sleep -Seconds 1
        Save-Window $hwnd "load-panel"
        Write-Host "selecting second save row"
        Click-Client $hwnd 300 146
        Start-Sleep -Seconds 1
        Write-Host "clicking load check"
        Click-Client $hwnd 334 436
        Start-Sleep -Seconds 20
        Save-Window $hwnd "load-after"
    }
}

Start-Sleep -Seconds ([Math]::Max(2, $Seconds - 32))
Send 'exit'
if (-not $p.WaitForExit(10000)) { $p.Kill() }
$out = $outTask.Result
$err = $errTask.Result
$out | Set-Content -Path $logPath -Encoding utf8
$err | Set-Content -Path ($logPath -replace '\.out\.', '.err.') -Encoding utf8

Write-Host "--- loader/file matches ---"
Select-String -Path $logPath -Pattern "SKIN_LOADER|SKEL_LOADER|MESH_LOADER|\.skel|\.skin|\.anim|\.mesh|InsertCD|EXCEPTION" | Select-Object -Last 120
