# Retail in-park "Quit Game" flow probe.
#
# Starts the retail game from run-test in the background with virtual input,
# reaches a Free Game park, opens the in-park OPTIONS panel, clicks the
# Quit Game icon, and then either confirms or cancels the REQUEST dialog.
# Screenshots go to notes\harness (gitignored). run-test\save is snapshotted
# (name, size, mtime, SHA-256) around the dialog so a quit-time save can be
# detected.
#
# Background-safe: no SetCursorPos / mouse_event / SetForegroundWindow. The
# game is sent to the back after launch and screenshots use PrintWindow.
param(
    [ValidateSet('confirm', 'cancel')]
    [string]$Action = 'confirm',
    [string]$GameDir = 'run-test',
    [string]$ShotDir = 'notes\harness',
    [string]$SnapDir = 'notes\harness',
    [string]$Log = '',
    [int]$WindowTimeoutSeconds = 50,
    [switch]$Direct
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
$wd = Join-Path $root $GameDir
$exe = Join-Path $wd 'DinoIsland.exe'
$hookLog = Join-Path $wd 'dino_hook.log'
$inputPath = Join-Path $wd 'dino_input.txt'
$saveDir = Join-Path $wd 'save'
$shotRoot = Join-Path $root $ShotDir
$snapRoot = Join-Path $root $SnapDir
$tag = "quit-$Action"
$timelinePath = Join-Path $snapRoot "$tag-timeline.txt"
if (-not $Log) {
    $Log = "notes\harness\$tag-x32dbg.out.log"
}
$logPath = Join-Path $root $Log

New-Item -ItemType Directory -Force $shotRoot | Out-Null
New-Item -ItemType Directory -Force $snapRoot | Out-Null

. (Join-Path $root 'scripts\tools.ps1')
. (Join-Path $PSScriptRoot 'VirtualInput.ps1')

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DinoQuitWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
}
"@
Add-Type -AssemblyName System.Drawing

function Now-Ms { [Environment]::TickCount }
function Write-Timeline([string]$text) {
    $line = '{0,10} {1}' -f (Now-Ms), $text
    Add-Content -Path $timelinePath -Value $line
    Write-Host $line
}

function Save-Shot([IntPtr]$hwnd, [string]$name) {
    if ($hwnd -eq [IntPtr]::Zero -or -not [DinoQuitWin]::IsWindow($hwnd)) {
        Write-Timeline "shot $name skipped (window gone)"
        return
    }
    [DinoQuitWin]::ShowWindow($hwnd, 4) | Out-Null
    Start-Sleep -Milliseconds 150
    $r = New-Object DinoQuitWin+RECT
    [DinoQuitWin]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    $w = $r.Right - $r.Left
    $h = $r.Bottom - $r.Top
    if ($w -le 0 -or $h -le 0) {
        Write-Timeline "shot $name skipped (empty rect)"
        return
    }
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    try {
        $dc = $g.GetHdc()
        [DinoQuitWin]::PrintWindow($hwnd, $dc, 0) | Out-Null
        $g.ReleaseHdc($dc)
    } finally { $g.Dispose() }
    $path = Join-Path $shotRoot "$name.png"
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Timeline "shot $name -> $path"
}

function Save-Snapshot([string]$label) {
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("# run-test/save snapshot '$label' at $(Get-Date -Format o)")
    if (-not (Test-Path $saveDir)) {
        $lines.Add('# save directory missing')
    } else {
        $dirs = Get-ChildItem $saveDir -Directory -Recurse -Force | Sort-Object FullName
        foreach ($d in $dirs) {
            $rel = $d.FullName.Substring($saveDir.Length + 1)
            $lines.Add("D|$rel|$($d.LastWriteTime.ToString('o'))")
        }
        $files = Get-ChildItem $saveDir -File -Recurse -Force | Sort-Object FullName
        foreach ($f in $files) {
            $rel = $f.FullName.Substring($saveDir.Length + 1)
            $hash = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash
            $lines.Add("F|$rel|$($f.Length)|$($f.LastWriteTime.ToString('o'))|$hash")
        }
    }
    $path = Join-Path $snapRoot "$tag-save-$label.txt"
    Set-Content -Path $path -Value $lines -Encoding utf8
    Write-Timeline "save snapshot $label -> $path ($($lines.Count - 1) entries)"
}

function Get-ButtonEventCount {
    if (-not (Test-Path $hookLog)) { return 0 }
    return @(Select-String -Path $hookLog -Pattern 'state button=').Count
}

function Get-ResetCount {
    if (-not (Test-Path $hookLog)) { return 0 }
    return @(Select-String -Path $hookLog -Pattern 'Reset hr=0x0').Count
}

function Wait-ParkReady([int]$timeoutSeconds, [int]$resetsBefore = -1) {
    $before = if ($resetsBefore -ge 0) { $resetsBefore } else { Get-ResetCount }
    $deadline = (Get-Date).AddSeconds($timeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
        $now = Get-ResetCount
        if ($now -gt $before) {
            Write-Timeline "park D3D Reset observed ($before -> $now)"
            Start-Sleep -Seconds 3
            return $true
        }
        $g = Get-Process -Id $gamePid -ErrorAction SilentlyContinue
        if (-not $g) { throw 'DinoIsland exited during the park load' }
    }
    Write-Timeline "no park D3D Reset after $timeoutSeconds s; continuing after a fixed wait"
    Start-Sleep -Seconds 8
    return $false
}

function Get-GameState {
    if (-not $gamePid) { return 'no pid' }
    $g = Get-Process -Id $gamePid -ErrorAction SilentlyContinue
    if (-not $g) { return "pid=$gamePid process-exited" }
    $g.Refresh()
    return ("pid={0} hwnd={1} title='{2}' responding={3}" -f `
        $g.Id, $g.MainWindowHandle, $g.MainWindowTitle, $g.Responding)
}

function Click-At([IntPtr]$hwnd, [int]$x, [int]$y, [string]$what) {
    $before = Get-ButtonEventCount
    Write-Timeline "click $what ($x,$y) state_button_before=$before"
    Invoke-VirtualClick -Hwnd $hwnd -X $x -Y $y -GameDir $wd
    Start-Sleep -Milliseconds 250
    $after = Get-ButtonEventCount
    Write-Timeline "click $what delivered state_button_after=$after delta=$($after - $before)"
}

function Get-ClientOffset([IntPtr]$hwnd) {
    $pt = New-Object DinoQuitWin+POINT
    $pt.X = 0
    $pt.Y = 0
    [DinoQuitWin]::ClientToScreen($hwnd, [ref]$pt) | Out-Null
    $r = New-Object DinoQuitWin+RECT
    [DinoQuitWin]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    return @(($pt.X - $r.Left), ($pt.Y - $r.Top))
}

function Click-ParkView([IntPtr]$hwnd, [int]$x, [int]$y, [string]$what) {
    # The park HUD hit-tests in window space because its view does not use the
    # menu's offset-corrected mouse-move hook. Pre-compensate by the frame
    # offset (client origin - window origin): 3,26 on the bordered test window.
    $off = Get-ClientOffset $hwnd
    $cx = $x - $off[0]
    $cy = $y - $off[1]
    Write-Timeline "park-view $what view=($x,$y) client=($cx,$cy) frame=$($off[0]),$($off[1])"
    Click-At $hwnd $cx $cy $what
}

function Wait-GameWindow([int]$timeoutSeconds) {
    $deadline = (Get-Date).AddSeconds($timeoutSeconds)
    $lastNudge = Now-Ms
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
        if ($proc -and $proc.HasExited) {
            throw "DinoIsland exited before opening a window (code $($proc.ExitCode))"
        }
        if ($dbg -and ((Now-Ms) - $lastNudge) -gt 2500) {
            # If the debugger stopped on an entry/TLS stop, keep it moving.
            try {
                $dbg.StandardInput.WriteLine('run')
                $dbg.StandardInput.Flush()
            } catch { }
            $lastNudge = Now-Ms
        }
        $g = Get-Process -Name DinoIsland -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($g) {
            $g.Refresh()
            if ($g.MainWindowHandle -ne [IntPtr]::Zero) {
                $script:gamePid = $g.Id
                return $g.MainWindowHandle
            }
        }
    }
    throw "no DinoIsland main window after $timeoutSeconds s"
}

# --- preflight ------------------------------------------------------------
$existing = Get-Process DinoIsland -ErrorAction SilentlyContinue
if ($existing) {
    throw ("DinoIsland already running (PID {0}); refusing to touch another lane's game" -f `
        (($existing | ForEach-Object { $_.Id }) -join ','))
}
$existingDbg = Get-Process headless -ErrorAction SilentlyContinue
if ($existingDbg) {
    throw ("x32dbg headless already running (PID {0}); refusing to touch another lane" -f `
        (($existingDbg | ForEach-Object { $_.Id }) -join ','))
}

if (Test-Path $hookLog) { Move-Item $hookLog "$hookLog.prev" -Force }
Remove-Item $timelinePath, $inputPath -ErrorAction SilentlyContinue
Set-Content -Path $inputPath -Value '400 300 0' -NoNewline
if ($proc) { $proc = $null }

$env:DINO_VIRTUAL_MOUSE = '1'
$env:DINO_FORCE_INPUT_ACTIVE = '1'
$env:DINO_MOUSE_CLICK_CAPTURE = '0'
$env:DINO_LOG_ERRORS = '1'
Remove-Item Env:DINO_VIRTUAL_KEYS -ErrorAction SilentlyContinue
Remove-Item Env:DINO_AUTO_DISMISS_ERRORS -ErrorAction SilentlyContinue
Remove-Item Env:DINO_CAMERA_PROBE -ErrorAction SilentlyContinue

$previousForeground = [DinoVirtualInputShared]::GetForegroundWindow()
Write-Timeline "previous foreground hwnd=$previousForeground"

$dbg = $null
$outTask = $null
$errTask = $null

if ($Direct) {
    $stdout = Join-Path $wd "$tag.out"
    $stderr = Join-Path $wd "$tag.err"
    $proc = Start-Process -FilePath $exe -WorkingDirectory $wd -PassThru `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    Write-Timeline "direct launch pid=$($proc.Id)"
} else {
    $h = Get-X32dbgHeadless
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $h
    $psi.Arguments = "-workingDir `"$wd`" `"$exe`""
    $psi.UseShellExecute = $false
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $psi.WorkingDirectory = $root
    $dbg = [System.Diagnostics.Process]::Start($psi)
    $outTask = $dbg.StandardOutput.ReadToEndAsync()
    $errTask = $dbg.StandardError.ReadToEndAsync()
    Write-Timeline "x32dbg headless pid=$($dbg.Id)"

    $script:Send = {
        param([string]$line)
        $dbg.StandardInput.WriteLine($line)
        $dbg.StandardInput.Flush()
    }
    & $script:Send 'bc'
    for ($i = 0; $i -lt 10; $i++) {
        & $script:Send 'run'
        Start-Sleep -Milliseconds 900
    }
}

$hwnd = Wait-GameWindow $WindowTimeoutSeconds
Write-Timeline "game window hwnd=$hwnd ($(Get-GameState))"

if (-not $Direct) {
    # Make sure the debugger is running, not paused at the entry stop.
    & $script:Send 'pause'
    Start-Sleep -Milliseconds 700
    & $script:Send 'run'
    Start-Sleep -Milliseconds 1200
}

& (Join-Path $root 'scripts\Set-WindowedStyle.ps1') -Hwnd $hwnd -SendToBack
Restore-Foreground $previousForeground
[DinoQuitWin]::ShowWindow($hwnd, 4) | Out-Null
Start-Sleep -Milliseconds 500
Write-Timeline ("window styled and sent to back; foreground restored hwnd={0}" -f `
    [DinoVirtualInputShared]::GetForegroundWindow())

# --- retail menu -> Free Game park ---------------------------------------
Start-Sleep -Seconds 8
Save-Shot $hwnd 'quit-menu'

Click-At $hwnd 395 234 'FREE MODE'
Start-Sleep -Seconds 2
Save-Shot $hwnd 'quit-freegame'

Click-At $hwnd 260 183 'IGUANAO'
Start-Sleep -Milliseconds 1200
Click-At $hwnd 494 451 'easy difficulty'
Start-Sleep -Milliseconds 1200
Save-Shot $hwnd 'quit-freegame-selected'

$resetsBeforeStart = Get-ResetCount
Click-At $hwnd 526 531 'Start'
Start-Sleep -Seconds 2
Save-Shot $hwnd 'quit-park-loading'
$parkReady = Wait-ParkReady 25 $resetsBeforeStart
Start-Sleep -Seconds 1
Save-Shot $hwnd 'quit-park'
$fgAfterPark = [DinoVirtualInputShared]::GetForegroundWindow()
Write-Timeline ("park state {0} parkReady={1} foreground={2} game_is_foreground={3}" -f `
    (Get-GameState), $parkReady, $fgAfterPark, ($fgAfterPark -eq $hwnd))
Start-Sleep -Seconds 1

# --- quit dialog ----------------------------------------------------------
Save-Snapshot 'before-dialog'
Click-ParkView $hwnd 765 348 'OPEN_OPTIONS'
Start-Sleep -Milliseconds 1200
Save-Shot $hwnd 'quit-options'

Click-ParkView $hwnd 447 74 'OPTIONS_MENU_EXIT_ICON'
Start-Sleep -Seconds 2
Save-Shot $hwnd 'quit-confirm'
Save-Shot $hwnd "quit-dialog-$Action"
Save-Snapshot 'dialog-open'
Start-Sleep -Milliseconds 500
Save-Shot $hwnd 'quit-confirm-settled'
Write-Timeline "dialog state $(Get-GameState)"

if ($Action -eq 'cancel') {
    Click-ParkView $hwnd 414 370 'REQUEST_CANCEL_ICON'
    Start-Sleep -Milliseconds 1800
    Save-Shot $hwnd 'quit-cancel'
    Save-Snapshot 'after-cancel'
    Start-Sleep -Milliseconds 500
    Save-Shot $hwnd 'quit-cancel-settled'
    Write-Timeline "cancel state $(Get-GameState)"

    # Close the options panel and capture the still-running park.
    Click-ParkView $hwnd 355 432 'OPTIONS close'
    Start-Sleep -Milliseconds 1200
    Save-Shot $hwnd 'quit-cancel-park'
    Save-Snapshot 'cancel-park'
    Write-Timeline "cancel park state $(Get-GameState)"
} else {
    Click-ParkView $hwnd 384 370 'REQUEST_VALIDATE_ICON'
    Start-Sleep -Seconds 2
    Save-Shot $hwnd 'quit-after-confirm'
    Save-Snapshot 'after-confirm'
    Write-Timeline "confirm +2s state $(Get-GameState)"
    Start-Sleep -Seconds 4
    Save-Shot $hwnd 'quit-after-confirm-settled'
    Write-Timeline "confirm +6s state $(Get-GameState)"
    Start-Sleep -Seconds 6
    Save-Shot $hwnd 'quit-after-confirm-late'
    Save-Snapshot 'after-confirm-late'
    Write-Timeline "confirm +12s state $(Get-GameState)"
}

# Dump the delivered-click log slice for the report.
if (Test-Path $hookLog) {
    Select-String -Path $hookLog -Pattern 'state button=' |
        ForEach-Object { $_.Line } |
        Set-Content -Path (Join-Path $snapRoot "$tag-state-buttons.txt") -Encoding utf8
}

# --- cleanup --------------------------------------------------------------
$game = Get-Process -Id $gamePid -ErrorAction SilentlyContinue
if ($game) {
    Write-Timeline "stopping game pid=$gamePid"
    Stop-Process -Id $gamePid -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 800
}
if ($dbg) {
    try {
        $dbg.StandardInput.WriteLine('exit')
        $dbg.StandardInput.Flush()
    } catch { }
    if (-not $dbg.WaitForExit(8000)) {
        Write-Timeline "killing x32dbg headless pid=$($dbg.Id)"
        $dbg.Kill()
    }
    $out = $outTask.Result
    $err = $errTask.Result
    $out | Set-Content -Path $logPath -Encoding utf8
    $errPath = if ($logPath -match '\.out\.') {
        $logPath -replace '\.out\.', '.err.'
    } else {
        [System.IO.Path]::ChangeExtension($logPath, '.err.log')
    }
    $err | Set-Content -Path $errPath -Encoding utf8
}

Restore-Foreground $previousForeground
$leftGame = Get-Process -Name DinoIsland -ErrorAction SilentlyContinue
$leftDbg = Get-Process -Name headless -ErrorAction SilentlyContinue
Write-Timeline ("final processes: DinoIsland={0} headless={1}; foreground restored hwnd={2}" -f `
    (($leftGame | ForEach-Object { $_.Id }) -join ','), `
    (($leftDbg | ForEach-Object { $_.Id }) -join ','), `
    ([DinoQuitWin]::GetForegroundWindow()))
Write-Host "timeline: $timelinePath"
Write-Host "hook log: $hookLog"
