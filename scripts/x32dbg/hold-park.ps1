# Diagnostic hold: launch the retail free-game park in the background and
# leave it running so another shell can probe window messages / clicks.
# Virtual input only; no physical cursor movement.
param(
    [string]$GameDir = 'run-test',
    [int]$Seconds = 300,
    [switch]$SkipRestart
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
$wd = Join-Path $root $GameDir
$exe = Join-Path $wd 'DinoIsland.exe'
$hookLog = Join-Path $wd 'dino_hook.log'
$inputPath = Join-Path $wd 'dino_input.txt'

. (Join-Path $root 'scripts\tools.ps1')
. (Join-Path $PSScriptRoot 'VirtualInput.ps1')

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DinoHoldWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
}
"@
Add-Type -AssemblyName System.Drawing

function Now-Ms { [Environment]::TickCount }
function Log([string]$text) {
    $line = '{0,10} {1}' -f (Now-Ms), $text
    Add-Content -Path (Join-Path $wd 'hold-park-timeline.txt') -Value $line
    Write-Host $line
}
function Save-Shot([IntPtr]$hwnd, [string]$name) {
    if ($hwnd -eq [IntPtr]::Zero) { return }
    [DinoHoldWin]::ShowWindow($hwnd, 4) | Out-Null
    Start-Sleep -Milliseconds 150
    $r = New-Object DinoHoldWin+RECT
    [DinoHoldWin]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $w = $r.Right - $r.Left
    $h = $r.Bottom - $r.Top
    if ($w -le 0 -or $h -le 0) { return }
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    try {
        $dc = $g.GetHdc()
        [DinoHoldWin]::PrintWindow($hwnd, $dc, 0) | Out-Null
        $g.ReleaseHdc($dc)
    } finally { $g.Dispose() }
    $dir = Join-Path $root 'notes\harness'
    New-Item -ItemType Directory -Force $dir | Out-Null
    $bmp.Save((Join-Path $dir "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}
function Get-ResetCount {
    if (-not (Test-Path $hookLog)) { return 0 }
    return @(Select-String -Path $hookLog -Pattern 'Reset hr=0x0').Count
}

$existing = Get-Process DinoIsland -ErrorAction SilentlyContinue
if ($existing -and -not $SkipRestart) {
    throw ("DinoIsland already running (PID {0}); refusing to kill another lane" -f `
        (($existing | ForEach-Object { $_.Id }) -join ','))
}

$env:DINO_VIRTUAL_MOUSE = '1'
$env:DINO_FORCE_INPUT_ACTIVE = '1'
$env:DINO_MOUSE_CLICK_CAPTURE = '0'
$env:DINO_LOG_ERRORS = '1'
Remove-Item Env:DINO_VIRTUAL_KEYS -ErrorAction SilentlyContinue

$previousForeground = [DinoVirtualInputShared]::GetForegroundWindow()
Remove-Item (Join-Path $wd 'hold-park-timeline.txt') -ErrorAction SilentlyContinue
Set-Content -Path $inputPath -Value '400 300 0' -NoNewline

$p = Start-Process -FilePath $exe -WorkingDirectory $wd -PassThru `
    -RedirectStandardOutput (Join-Path $wd 'hold-park.out') `
    -RedirectStandardError (Join-Path $wd 'hold-park.err')
Log "launch pid=$($p.Id) previous_fg=$previousForeground"

$hwnd = [IntPtr]::Zero
$deadline = (Get-Date).AddSeconds(40)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    $p.Refresh()
    if ($p.HasExited) { throw "game exited early (code $($p.ExitCode))" }
    if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $hwnd = $p.MainWindowHandle; break }
}
if ($hwnd -eq [IntPtr]::Zero) { throw 'no game window' }
Log "window hwnd=$hwnd"
& (Join-Path $root 'scripts\Set-WindowedStyle.ps1') -Hwnd $hwnd -SendToBack
Restore-Foreground $previousForeground
[DinoHoldWin]::ShowWindow($hwnd, 4) | Out-Null
Start-Sleep -Seconds 8
Save-Shot $hwnd 'hold-menu'

Invoke-VirtualClick -Hwnd $hwnd -X 395 -Y 234 -GameDir $wd
Start-Sleep -Seconds 2
Invoke-VirtualClick -Hwnd $hwnd -X 260 -Y 183 -GameDir $wd
Start-Sleep -Milliseconds 1200
Invoke-VirtualClick -Hwnd $hwnd -X 494 -Y 451 -GameDir $wd
Start-Sleep -Milliseconds 1200
$resetBefore = Get-ResetCount
Invoke-VirtualClick -Hwnd $hwnd -X 526 -Y 531 -GameDir $wd
Start-Sleep -Seconds 2
$deadline = (Get-Date).AddSeconds(25)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if ((Get-ResetCount) -gt $resetBefore) { break }
}
Start-Sleep -Seconds 4
Save-Shot $hwnd 'hold-park'
Log ("park hwnd=$hwnd fg={0} game_fg={1} reset={2}" -f `
    [DinoHoldWin]::GetForegroundWindow(), ([DinoHoldWin]::GetForegroundWindow() -eq $hwnd), `
    (Get-ResetCount))
Log "holding for $Seconds s; pid=$($p.Id)"
Start-Sleep -Seconds $Seconds
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Restore-Foreground $previousForeground
Log "killed pid=$($p.Id); foreground restored"
