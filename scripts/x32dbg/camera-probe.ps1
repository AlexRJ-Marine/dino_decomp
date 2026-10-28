# Retail camera probe: launch the free-game park behind the current window,
# drive the camera keys through the DINPUT8 virtual keyboard, and record the
# live camera object (DINO_CAMERA_PROBE) to run-test\dino_camera.csv.
#
# Background-safe: SW_SHOWNOACTIVATE, virtual clicks (dino_input.txt), and
# virtual keys (dino_keys.txt). No SetCursorPos / mouse_event / SetForegroundWindow.
param(
    [string]$GameDir = "run-test",
    [int]$LoadTimeoutSeconds = 40,
    [switch]$SkipRestart
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
$wd = Join-Path $root $GameDir
$exe = Join-Path $wd "DinoIsland.exe"
$keyPath = Join-Path $wd "dino_keys.txt"
$timelinePath = Join-Path $wd "camera-probe-timeline.txt"
$csvPath = Join-Path $wd "dino_camera.csv"
$hookLog = Join-Path $wd "dino_hook.log"
$shotDir = Join-Path $root "notes\harness"

if (-not (Test-Path $exe)) {
    throw "missing $exe; run scripts\setup-test-runtime.ps1 first"
}

. (Join-Path $root "scripts\x32dbg\VirtualInput.ps1")

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DinoCamProbeWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
}
"@
Add-Type -AssemblyName System.Drawing

function Now-Ms { [Environment]::TickCount }
function Log([string]$text) {
    $line = "{0,10} {1}" -f (Now-Ms), $text
    Add-Content -Path $timelinePath -Value $line
    Write-Host $line
}
function Set-Keys([string]$codes) {
    Set-Content -Path $keyPath -Value $codes -NoNewline -Encoding ascii
}
function Hold-Keys([string]$name, [string]$codes, [int]$milliseconds) {
    Log "key-down $name [$codes]"
    Set-Keys $codes
    Start-Sleep -Milliseconds $milliseconds
    Set-Keys ""
    Log "key-up   $name"
    Start-Sleep -Milliseconds 1300
}

# Move the virtual cursor without a button. The proxy feeds the relative delta
# from dino_input.txt to the engine's ring; WM_MOUSEMOVE keeps the UI cursor
# in sync. Used to test retail edge scrolling / mouse camera motion.
function Move-Mouse([string]$name, [int]$x, [int]$y, [int]$milliseconds) {
    Log "mouse-move $name ($x,$y)"
    $inputPath = Join-Path $wd "dino_input.txt"
    Set-Content -Path $inputPath -Value ("{0} {1} 0" -f $x, $y) -NoNewline
    $lParam = [IntPtr](($y -shl 16) -bor ($x -band 0xffff))
    [DinoCamProbeWin]::PostMessage($hwnd, 0x200, [IntPtr]::Zero, $lParam) | Out-Null
    Start-Sleep -Milliseconds $milliseconds
    # Park the cursor in the middle between tests.
    Set-Content -Path $inputPath -Value "400 300 0" -NoNewline
    [DinoCamProbeWin]::PostMessage(
        $hwnd, 0x200, [IntPtr]::Zero, [IntPtr]((300 -shl 16) -bor 400)) | Out-Null
    Start-Sleep -Milliseconds 1200
}
function Save-Shot([IntPtr]$hwnd, [string]$name) {
    if ($hwnd -eq [IntPtr]::Zero) { return }
    [DinoCamProbeWin]::ShowWindow($hwnd, 4) | Out-Null
    Start-Sleep -Milliseconds 120
    $r = New-Object DinoCamProbeWin+RECT
    [DinoCamProbeWin]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $w = $r.Right - $r.Left
    $h = $r.Bottom - $r.Top
    if ($w -le 0 -or $h -le 0) { return }
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    try {
        $dc = $g.GetHdc()
        [DinoCamProbeWin]::PrintWindow($hwnd, $dc, 0) | Out-Null
        $g.ReleaseHdc($dc)
    } finally { $g.Dispose() }
    New-Item -ItemType Directory -Force $shotDir | Out-Null
    $bmp.Save((Join-Path $shotDir "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

# --- launch ---------------------------------------------------------------
$env:DINO_VIRTUAL_MOUSE = "1"
$env:DINO_VIRTUAL_KEYS = "1"
$env:DINO_FORCE_INPUT_ACTIVE = "1"
$env:DINO_CAMERA_PROBE = "1"
$env:DINO_MOUSE_CLICK_CAPTURE = "0"

Remove-Item $csvPath, $timelinePath -ErrorAction SilentlyContinue
Remove-Item $keyPath -ErrorAction SilentlyContinue
Set-Keys ""

if (-not $SkipRestart) {
    Get-Process DinoIsland -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -like "$wd*" } | Stop-Process -Force
    Start-Sleep -Milliseconds 500
}

if (Test-Path $hookLog) { Move-Item $hookLog "$hookLog.prev" -Force }

$p = Start-Process -FilePath $exe -WorkingDirectory $wd -PassThru `
    -RedirectStandardOutput (Join-Path $wd "camera-probe.out") `
    -RedirectStandardError (Join-Path $wd "camera-probe.err")
Log "launch pid=$($p.Id)"

$hwnd = [IntPtr]::Zero
$deadline = (Get-Date).AddSeconds($LoadTimeoutSeconds)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    $p.Refresh()
    if ($p.HasExited) { throw "game exited early (code $($p.ExitCode))" }
    if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $hwnd = $p.MainWindowHandle; break }
}
if ($hwnd -eq [IntPtr]::Zero) { throw "no game window after $LoadTimeoutSeconds s" }
Log "window hwnd=$hwnd"
& (Join-Path $root "scripts\Set-WindowedStyle.ps1") -Hwnd $hwnd -SendToBack
[DinoCamProbeWin]::ShowWindow($hwnd, 4) | Out-Null
Start-Sleep -Seconds 8
Save-Shot $hwnd "cam-probe-menu"

# --- free game: PLAY -> IGUANO -> easy -> Start --------------------------
Log "click PLAY (395,234)"
Invoke-VirtualClick -Hwnd $hwnd -X 395 -Y 234 -GameDir $wd
Start-Sleep -Seconds 3
Log "click IGUANO (260,183)"
Invoke-VirtualClick -Hwnd $hwnd -X 260 -Y 183 -GameDir $wd
Start-Sleep -Seconds 2
Log "click easy (494,451)"
Invoke-VirtualClick -Hwnd $hwnd -X 494 -Y 451 -GameDir $wd
Start-Sleep -Seconds 2
Save-Shot $hwnd "cam-probe-selected"
Log "click Start (526,531)"
Invoke-VirtualClick -Hwnd $hwnd -X 526 -Y 531 -GameDir $wd
$parkDeadline = (Get-Date).AddSeconds(30)
while ((Get-Date) -lt $parkDeadline) {
    Start-Sleep -Seconds 1
    if (Test-Path $csvPath) { break }
}
if (-not (Test-Path $csvPath)) {
    Log "no camera probe after 30 s; clicking Start again"
    Invoke-VirtualClick -Hwnd $hwnd -X 526 -Y 531 -GameDir $wd
    Start-Sleep -Seconds 20
}
Save-Shot $hwnd "cam-probe-park"
Log "park settle"
Start-Sleep -Seconds 3

# --- camera key segments -------------------------------------------------
Log "baseline 2s"
Start-Sleep -Seconds 2

# Mouse camera / edge-scroll tests: hold the virtual cursor at each edge.
Move-Mouse "right-edge"  799 300 2500
Move-Mouse "left-edge"     1 300 2500
Move-Mouse "bottom-edge" 400 599 2500
Move-Mouse "top-edge"    400   1 2500
Move-Mouse "bottom-right" 799 599 2500
Move-Mouse "bottom-left"    1 599 2500

Hold-Keys "Up"          "C8"       1000
Hold-Keys "Down"        "D0"       1000
Hold-Keys "Left"        "CB"       1000
Hold-Keys "Right"       "CD"       1000
Hold-Keys "Shift+Up"    "2A C8"    1000
Hold-Keys "Shift+Down"  "2A D0"    1000
Hold-Keys "Shift+Left"  "2A CB"    1000
Hold-Keys "Shift+Right" "2A CD"    1000
Hold-Keys "Home"        "C7"       1000
Hold-Keys "End"         "CF"       1000
Hold-Keys "Delete"      "D3"       1000
Hold-Keys "PageDown"    "D1"       1000
Hold-Keys "Pad+"        "4E"       1000
Hold-Keys "Pad-"        "4A"       1000
Hold-Keys "Up short"    "C8"       300
Hold-Keys "Up long"     "C8"       2000

Log "done"
Save-Shot $hwnd "cam-probe-final"
Start-Sleep -Seconds 1
Stop-Process -Id $p.Id -Force
Log "killed pid=$($p.Id)"

Write-Host "CSV: $csvPath"
Write-Host "timeline: $timelinePath"
Write-Host "hook log: $hookLog"
