param(
    [ValidateSet('Mark', 'Hw')][string]$Mode = 'Hw',
    [int]$CampaignRow = 3,
    [int]$MissionRow = 0,
    [int]$CellXMin = 420,
    [int]$CellXMax = 499,
    [int]$CellYMin = 416,
    [int]$CellYMax = 451,
    [int]$HwSeconds = 20,
    [int]$MaxReads = 20000,
    [int]$ParkTimeoutSec = 180,
    [int]$SimSeconds = 12,
    [switch]$UnlockCampaigns,
    [switch]$AnyCell,
    [switch]$NoExitDelay,
    [string]$GameDir = "run-test",
    [string]$Log = ""
)

# Show-arena wall grid probe (read-only).
#
# Question: after FUN_005849a0 stamps the show perimeter cells class 7
# (DAT_006757b4<<12), does any display/scene function read such a cell and
# instantiate the wall mesh, or is the stamp occupancy-only?
#
# Static anchors (Ghidra /DinoIsland.exe-ebea17):
#   0x5849a0  per-stWallCell occupancy commit
#   0x584a12  AND word [EBP+EDI*2],0xfff  -> ECX=cell, EBP=rows[x], EDI=y,
#             ESI=x, EAX=DAT_006757b4<<12 (0x7000); cell address = EBP+2*EDI
#   0x584a23  OR  word [ESI+EDI*2],AX     -> the stamp
#   0x5844fc  MOV ECX,[EDX+ESI*4]         -> per-cell-record loop in FUN_00584200
#
# Mode Mark: log every stamped point (census; proves which rectangles load).
# Mode Hw:   stop at the first stamp inside -Cell* (default the Cpg04 show
#            rect (420,416)-(499,451)), then arm bph <cell>, r, 2 and log the
#            readers. The stop is real (a break condition), so the debuggee is
#            paused and the arm commands are processed immediately.
#
# IMPORTANT harness facts found while building this:
#   * x64dbg expressions treat bare numbers as HEX ({d:420} = 1056). Decimal
#     bounds must be spelled 0x... (0n... is not supported by this build).
#   * x64dbg executes a breakpoint command even when the break condition is
#     false, then continues; only a true condition stops.
#   * Commands sent while the debuggee runs are queued until the next stop.
#   * PrintWindow/ShowWindow can block while the debuggee is stopped at a
#     breakpoint (the window thread is frozen), so this probe takes no
#     screenshots and uses PostMessage-only keep-alive while waiting.
#
# Background-safe: virtual input only (DINPUT8 proxy + run-test/dino_input.txt),
# no SetCursorPos / mouse_event / SetForegroundWindow.

$root = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
. (Join-Path $root "scripts\tools.ps1")
$h = Get-X32dbgHeadless
$wd = Join-Path $root $GameDir
$exe = Join-Path $wd "DinoIsland.exe"
if (-not $Log) { $Log = "notes\harness\show-wall-grid-$Mode.log" }
$logPath = Join-Path $root $Log
$dinoDat = Join-Path $wd "save\dino.dat"
$dinoDatBackup = $null

$env:DINO_VIRTUAL_MOUSE = "1"
$env:DINO_FORCE_INPUT_ACTIVE = "1"
$env:DINO_VIRTUAL_KEYS = "1"
. (Join-Path $PSScriptRoot "VirtualInput.ps1")
$previousForeground = [DinoVirtualInputShared]::GetForegroundWindow()

if ($UnlockCampaigns -and (Test-Path $dinoDat)) {
    $dinoDatBackup = [System.IO.File]::ReadAllBytes($dinoDat)
    [System.IO.File]::WriteAllBytes($dinoDat, [byte[]](8, 0, 0, 0))
    Write-Host "unlocked all campaigns (dino.dat = 8, backed up)"
}

Add-Type @"
using System;
using System.Collections.Concurrent;
using System.Runtime.InteropServices;
public class WgWin32 {
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
public class WgOutput {
  public static readonly ConcurrentQueue<string> Lines = new ConcurrentQueue<string>();
  public static void Pump(System.IO.StreamReader reader) {
    var thread = new System.Threading.Thread(() => {
      string line;
      while ((line = reader.ReadLine()) != null) { Lines.Enqueue(line); }
    });
    thread.IsBackground = true;
    thread.Start();
  }
  public static string[] Snapshot() { return Lines.ToArray(); }
}
"@

$script:gameHwnd = [IntPtr]::Zero
$script:seen = 0
$script:rawLines = @()
$script:markLines = @()
$script:readLines = @()
$hookLog = Join-Path $wd "dino_hook.log"

# Incremental log: a timeout or a killed process must not lose the trace.
Set-Content -Path $logPath -Value ("# show-wall-grid probe mode=$Mode {0}" -f (Get-Date).ToString('s')) -Encoding utf8

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
[WgOutput]::Pump($p.StandardOutput)
$errTask = $p.StandardError.ReadToEndAsync()

function Send([string]$line) {
    $p.StandardInput.WriteLine($line)
    $p.StandardInput.Flush()
}

function Read-NewLines {
    $all = [WgOutput]::Snapshot()
    if ($all.Count -le $script:seen) { return @() }
    $new = @($all[$script:seen..($all.Count - 1)])
    $script:seen = $all.Count
    return $new
}

function Collect-Log {
    $new = Read-NewLines
    if ($new.Count -gt 0) {
        Add-Content -Path $logPath -Value $new -Encoding utf8
        foreach ($line in $new) {
            $script:rawLines += $line
            if ($line -match 'WALLMARK') { $script:markLines += $line }
            if ($line -match 'CELLSTOP') { $script:markLines += $line }
            if ($line -match 'CELLREC') { $script:markLines += ('REC ' + $line) }
            if ($line -match 'CELLREAD') { $script:readLines += $line }
            if ($line -match 'Invalid|ERROR|EXCEPTION') { Write-Host ("debugger: " + $line.Trim()) }
        }
    }
}

function Get-GameProcess {
    $procs = @(Get-Process DinoIsland -ErrorAction SilentlyContinue)
    if ($procs.Count -eq 0) { return $null }
    if ($procs.Count -eq 1) { return $procs[0] }
    Write-Host ("warning: {0} DinoIsland processes running; using the newest" -f $procs.Count)
    return ($procs | Sort-Object StartTime | Select-Object -Last 1)
}

function Update-GameWindow {
    $proc = Get-GameProcess
    if ($null -eq $proc) { return $false }
    $hw = $proc.MainWindowHandle
    if ($hw -ne [IntPtr]::Zero -and $hw -ne $script:gameHwnd) { $script:gameHwnd = $hw }
    return [WgWin32]::IsWindow($script:gameHwnd)
}

function Post-Keepalive {
    # PostMessage only: async, safe while the debuggee is stopped.
    if (-not (Update-GameWindow)) { return }
    [WgWin32]::PostMessage($script:gameHwnd, 0x001C, [IntPtr]1, [IntPtr]::Zero) | Out-Null
    [WgWin32]::PostMessage($script:gameHwnd, 0x0006, [IntPtr]1, [IntPtr]::Zero) | Out-Null
    [WgWin32]::PostMessage($script:gameHwnd, 0x0007, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
}

function Wake-GameWindow {
    # Used only while the debuggee runs (menu clicks): SW_SHOWNOACTIVATE and
    # posted activation messages; no foreground call, no pointer movement.
    for ($i = 0; $i -lt 4; $i++) {
        if (-not (Update-GameWindow)) { Start-Sleep -Milliseconds 200; continue }
        Post-Keepalive
        [WgWin32]::ShowWindow($script:gameHwnd, 4) | Out-Null
        Start-Sleep -Milliseconds 150
        $c = New-Object WgWin32+RECT
        [WgWin32]::GetClientRect($script:gameHwnd, [ref]$c) | Out-Null
        if (-not [WgWin32]::IsIconic($script:gameHwnd) -and $c.Right -gt 0 -and $c.Bottom -gt 0) { return $true }
    }
    return $false
}

function Get-InputLogOffset {
    if (-not (Test-Path $hookLog)) { return 0 }
    return (Get-Item $hookLog).Length
}

function Wait-InputDelivered([long]$offset, [int]$timeoutMs) {
    $deadline = (Get-Date).AddMilliseconds($timeoutMs)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 100
        if (-not (Test-Path $hookLog)) { continue }
        try { $fs = [System.IO.File]::Open($hookLog, 'Open', 'Read', 'ReadWrite') } catch { continue }
        try {
            if ($fs.Length -le $offset) { continue }
            $fs.Position = $offset
            $reader = New-Object System.IO.StreamReader($fs)
            if (($reader.ReadToEnd()) -match 'state button=') { return $true }
        } finally { $fs.Dispose() }
    }
    return $false
}

function Click-Client([int]$x, [int]$y) {
    for ($attempt = 1; $attempt -le 2; $attempt++) {
        Wake-GameWindow | Out-Null
        $offset = Get-InputLogOffset
        Invoke-VirtualClick -Hwnd $script:gameHwnd -X $x -Y $y -GameDir $wd
        if (Wait-InputDelivered -Offset $offset -TimeoutMs 1500) { return $true }
        Write-Host ("click {0},{1} not delivered (attempt {2})" -f $x, $y, $attempt)
        Start-Sleep -Milliseconds 400
    }
    return $false
}

function Send-VirtualKeys([string]$codes, [int]$holdMs) {
    # DIK codes through the DINPUT8 proxy (run-test/dino_keys.txt); the same
    # path scripts/x32dbg/start-campaign.ps1 uses for the Esc hand-off.
    $keys = Join-Path $wd "dino_keys.txt"
    Set-Content -Path $keys -Value $codes -NoNewline -Encoding ascii
    Start-Sleep -Milliseconds $holdMs
    Set-Content -Path $keys -Value "" -NoNewline -Encoding ascii
    Start-Sleep -Milliseconds 300
}

# ---- breakpoint command strings -------------------------------------------
$cellMark = 'log WALLMARK|wx={p:esi}|wy={p:edi}|wrow={p:ebp}|wval={x:[ebp+edi*2]}' +
    '|wcls={p:eax}|wobj={p:ecx}|wmesh={p:[ecx]}|wf20={p:[ecx+0x20]}|wf24={p:[ecx+0x24]}' +
    '|wret={p:[esp+0x14]}|t={d:GetTickCount()}; run'
$cellStop = 'log CELLSTOP|wx={p:esi}|wy={p:edi}|wrow={p:ebp}|wval={x:[ebp+edi*2]}' +
    '|wcls={p:eax}|wobj={p:ecx}|wmesh={p:[ecx]}|wf20={p:[ecx+0x20]}|wf24={p:[ecx+0x24]}' +
    '|wret={p:[esp+0x14]}|t={d:GetTickCount()}'
$cellRec = 'log CELLREC|rx={p:[[[edx+esi*4]+8]]}|ry={p:[[[edx+esi*4]+8]+4]}' +
    '|robj={p:[edx+esi*4]}|rmesh={p:[[edx+esi*4]]}|ri={p:esi}|t={d:GetTickCount()}; run'

Send ''
Send 'bc'
Send 'run'
Start-Sleep -Milliseconds 700
Send 'run'
Start-Sleep -Milliseconds 700
Send 'run'
Start-Sleep -Seconds 5

$game = Get-GameProcess
if (-not $game) {
    Write-Host "game process not found"
} else {
    if (-not (Update-GameWindow)) {
        Write-Host "no main window handle"
    } else {
        & (Join-Path $root "scripts\Set-WindowedStyle.ps1") -Hwnd $script:gameHwnd -SendToBack
        Restore-Foreground $previousForeground
        Start-Sleep -Milliseconds 400
        Write-Host ("game pid={0} hwnd=0x{1:X}" -f $game.Id, $script:gameHwnd.ToInt64())
        Wake-GameWindow | Out-Null
        Start-Sleep -Seconds 2
        Send 'run'
        Start-Sleep -Milliseconds 300

        Write-Host "clicking Campaigns"
        Click-Client 395 189 | Out-Null
        Start-Sleep -Seconds 2

        Write-Host "selecting campaign row $CampaignRow"
        $campaignY = 166 + $CampaignRow * 34 + 17
        Click-Client 404 $campaignY | Out-Null
        Start-Sleep -Seconds 1

        Write-Host "clicking campaign Start"
        Click-Client 365 530 | Out-Null
        Start-Sleep -Seconds 2

        Write-Host "selecting mission row $MissionRow"
        $missionY = 142 + $MissionRow * 17 + 8
        Click-Client 259 $missionY | Out-Null
        Start-Sleep -Seconds 1

        # Probes must be installed while the debuggee is stopped (commands sent
        # while running are queued until the next stop). The world import, the
        # fence walls and the show arena all run after the mission Start click.
        Send 'pause'
        Start-Sleep -Milliseconds 500
        if ($Mode -eq 'Mark') {
            Send 'bp 0x584a12'
            Send ('SetBreakpointCommand 0x584a12, "' + $cellMark + '"')
            Send 'bp 0x5844fc'
            Send ('SetBreakpointCommand 0x5844fc, "' + $cellRec + '"')
        } else {
            Send 'bp 0x5844fc'
            Send ('SetBreakpointCommand 0x5844fc, "' + $cellRec + '"')
            Send 'bp 0x584a12'
            if (-not $AnyCell) {
                # x64dbg bare numbers are hexadecimal; 0n is unsupported.
                $cond = ('esi>=0x{0:X} && esi<=0x{1:X} && edi>=0x{2:X} && edi<=0x{3:X}' -f `
                    $CellXMin, $CellXMax, $CellYMin, $CellYMax)
                Send ('SetBreakpointCondition 0x584a12, "' + $cond + '"')
            }
            Send ('SetBreakpointCommand 0x584a12, "' + $cellStop + '"')
        }
        Send 'run'
        Start-Sleep -Milliseconds 300

        Write-Host "clicking mission Start"
        Click-Client 521 449 | Out-Null

        $hwArmed = $false
        $hwAddr = ''
        if ($Mode -eq 'Hw') {
            $condText = 'x=0x{0:X}..0x{1:X} y=0x{2:X}..0x{3:X}' -f $CellXMin, $CellXMax, $CellYMin, $CellYMax
            if ($AnyCell) { $condText = 'any cell' }
            Write-Host "waiting for the first wall stamp of $condText"
            $deadline = (Get-Date).AddSeconds($ParkTimeoutSec)
            $stopLine = $null
            $tick = 0
            # A breakpoint command runs on false-condition hits too, so the
            # CELLSTOP line alone is not a stop. But only a true-condition hit
            # can carry coordinates inside -Cell*, and the debuggee stays
            # stopped there, so the first in-range CELLSTOP is the stop.
            while ((Get-Date) -lt $deadline -and -not $stopLine) {
                Start-Sleep -Milliseconds 200
                Collect-Log
                foreach ($line in $script:markLines) {
                    if ($line -notmatch 'CELLSTOP') { continue }
                    $sx = -1; $sy = -1
                    if ($line -match 'wx=([0-9A-Fa-f]+)') { $sx = [Convert]::ToUInt32($Matches[1], 16) }
                    if ($line -match 'wy=([0-9A-Fa-f]+)') { $sy = [Convert]::ToUInt32($Matches[1], 16) }
                    if ($AnyCell -or ($sx -ge $CellXMin -and $sx -le $CellXMax -and
                                      $sy -ge $CellYMin -and $sy -le $CellYMax)) {
                        $stopLine = $line
                        break
                    }
                }
                if ($stopLine) { break }
                $tick++
                if (($tick % 5) -eq 0) { Post-Keepalive }
            }
            if ($stopLine) {
                Write-Host ("CELLSTOP: " + $stopLine.Trim())
                $row = 0
                $cy = 0
                if ($stopLine -match 'wrow=([0-9A-Fa-f]+)') { $row = [Convert]::ToUInt32($Matches[1], 16) }
                if ($stopLine -match 'wy=([0-9A-Fa-f]+)') { $cy = [Convert]::ToUInt32($Matches[1], 16) }
                $cell = $row + 2 * $cy
                $hwAddr = '0x{0:X8}' -f $cell
                Write-Host ("cell row=0x{0:X8} y=0x{1:X} -> cell={2}" -f $row, $cy, $hwAddr)
                # The breakpoint stopped the debuggee, so these commands run now.
                # Drop the marking probes (the census is already in the Mark
                # run) and arm the 16-bit read watchpoint on the observed cell.
                Send 'bc 0x584a12'
                Send 'bc 0x5844fc'
                Send ('bph ' + $hwAddr + ', r, 2')
                $hwCmd = 'SetHardwareBreakpointCommand ' + $hwAddr + ', "log CELLREAD' +
                    '|rcip={p:cip}|rret={p:[esp]}|rs4={p:[esp+4]}|rs8={p:[esp+8]}' +
                    '|rsC={p:[esp+0xc]}|rval={x:[' + $hwAddr + ']}|t={d:GetTickCount()}; run"'
                Send $hwCmd
                # Load-completion marker: the park clock tick (FUN_0058c2f0) is
                # called once the park loop runs; it self-deletes after one hit.
                Send 'bp 0x58c2fe'
                Send 'SetBreakpointCommand 0x58c2fe, "log PARKLOOP|dt={p:[esp+8]}|t={d:GetTickCount()}; bc 0x58c2fe; run"'
                Start-Sleep -Milliseconds 300
                Send 'run'
                $hwArmed = $true
                Write-Host "hardware read watchpoint armed on $hwAddr (16-bit)"
                $hwDeadline = (Get-Date).AddSeconds($HwSeconds)
                $hwStart = Get-Date
                $escSent = $false
                while ((Get-Date) -lt $hwDeadline) {
                    Start-Sleep -Milliseconds 200
                    Collect-Log
                    if ($script:readLines.Count -gt $MaxReads) {
                        Write-Host ("read cap reached ({0}); stopping" -f $script:readLines.Count)
                        break
                    }
                    # Dismiss the mission briefing once the park loop has
                    # started (one Esc, as the show budget probe does).
                    if (-not $escSent -and
                        (@($script:rawLines | Where-Object { $_ -match 'PARKLOOP' }).Count -gt 0)) {
                        Start-Sleep -Milliseconds 1500
                        Send-VirtualKeys '01' 250
                        $escSent = $true
                        Write-Host ("sent Esc to close the briefing at {0:N0}s" -f ((Get-Date) - $hwStart).TotalSeconds)
                    }
                    Post-Keepalive
                }
            } else {
                Write-Host "no true-condition stop within $ParkTimeoutSec s"
            }
        } else {
            Write-Host "waiting for the first wall mark"
            $deadline = (Get-Date).AddSeconds($ParkTimeoutSec)
            $first = $null
            while ((Get-Date) -lt $deadline -and -not $first) {
                Start-Sleep -Milliseconds 200
                Collect-Log
                if ($script:markLines.Count -gt 0) { $first = $script:markLines[0] }
                Post-Keepalive
            }
            if ($first) { Write-Host ("first mark: " + $first.Trim()) }
            else { Write-Host "no wall mark within $ParkTimeoutSec s" }
            $settle = (Get-Date).AddSeconds($SimSeconds)
            $lastCount = -1
            while ((Get-Date) -lt $settle) {
                Start-Sleep -Milliseconds 300
                Collect-Log
                if ($script:markLines.Count -eq $lastCount -and (Get-Date).AddSeconds(4) -lt $settle) {
                    $settle = (Get-Date).AddSeconds(4)
                }
                $lastCount = $script:markLines.Count
                Post-Keepalive
            }
        }

        Collect-Log
        if ($hwArmed) {
            # pause is safe while stopped (no-op) and stops a running debuggee.
            Send 'pause'
            Start-Sleep -Milliseconds 300
            Send ('bphc ' + $hwAddr)
            Send 'bc 0x584a12'
            Send 'run'
            Start-Sleep -Milliseconds 500
        }
        Collect-Log
        if (-not $NoExitDelay) { Start-Sleep -Seconds 2 }
        Collect-Log
    }
}

Send 'exit'
if (-not $p.WaitForExit(15000)) { $p.Kill() }
Start-Sleep -Milliseconds 700
Collect-Log
if ($null -ne $dinoDatBackup) {
    [System.IO.File]::WriteAllBytes($dinoDat, $dinoDatBackup)
    Write-Host "restored dino.dat"
}
$err = $errTask.Result
$errPath = $logPath -replace '\.out\.', '.err.'
if ($errPath -eq $logPath) { $errPath = "$logPath.err" }
$err | Set-Content -Path $errPath -Encoding utf8

Write-Host "--- probe summary ---"
Write-Host ("log: {0}" -f $logPath)
$marks = @(Select-String -Path $logPath -Pattern 'WALLMARK|CELLSTOP')
$recs = @(Select-String -Path $logPath -Pattern 'CELLREC')
$reads = @(Select-String -Path $logPath -Pattern 'CELLREAD')
Write-Host ("WALLMARK/CELLSTOP={0} CELLREC={1} CELLREAD={2}" -f $marks.Count, $recs.Count, $reads.Count)
$marks | Select-Object -First 4 | ForEach-Object { $_.Line }
Write-Host "--- distinct reader cip ---"
$cip = @{}
foreach ($r in $reads) {
    if ($r.Line -match 'rcip=([0-9A-Fa-f]+)') { $cip[$Matches[1].ToUpper()] = 1 }
}
$cip.Keys | Sort-Object | ForEach-Object { $_ }

