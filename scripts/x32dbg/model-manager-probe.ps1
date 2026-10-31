param(
    [ValidateSet('Entry', 'Fn')][string]$Mode = 'Entry',
    [int]$CampaignRow = 3,
    [int]$MissionRow = 0,
    [int]$Seconds = 30,
    [int]$SimSeconds = 15,
    [int]$ParkTimeoutSec = 120,
    [switch]$UnlockCampaigns,
    [string]$Fn = '',
    [string]$GameDir = 'run-test',
    [string]$Log = ''
)

# Model-manager virtual probe for the .bfi display object (read-only).
#
# Question: FUN_005f4e10 dispatches through [design+0x84]'s vtable slot +0x28:
#
#   0x5f4e2a  CALL dword ptr [EAX+0x28]      EAX = [design+0x84]
#
# Which vtable/function is it, what are the arguments, and which records
# (display_type = design+0x28) reach it?
#
# Static anchors (Ghidra /DinoIsland.exe-ebea17):
#   0x5f4e10  MOV ECX,[ESP+8]            -> arg1 = manager (owner+0x3c)
#             MOV EAX,[ECX]              -> manager vtable
#             PUSH [ESI+0xdc], PUSH arg3, PUSH arg2
#             CALL [EAX+0x28]            ECX=manager, [esp]=arg2=design+0x84
#             FUN_005fc260(ECX=this+0xf4, result)
#   0x53aed7  MOV ECX,[ESI+0x5c]         -> arg4  (owner+0x5c)
#   0x53aeda  MOV EAX,[ESI+0x3c]         -> arg1  (owner+0x3c, the manager)
#   0x53aee2  LEA EDX,[EBP+0x84]         -> arg2 = design+0x84
#   0x53aee8  PUSH EDI                   -> arg3 = feature actor
#   0x53aee9  MOV ECX,[ECX+0x8]          -> this (FUN_005f4e10 ECX)
#   0x53aeee  CALL 0x005f4e10
#
# At 0x5f4e2a: ECX = the model-manager object (owner+0x3c, the first stack
# arg of FUN_005f4e10), EAX = [ECX] (its vtable), ESI = FUN_005f4e10's this.
# The three virtual arguments are at [esp], [esp+4], [esp+8]:
#   [esp]   = design+0x84  (the .bfi display record block)
#   [esp+4] = feature actor (FUN_00589020 result)
#   [esp+8] = this+0xdc
# The .bfi display_type is design+0x30 = [esp]-0x54 (the `+0x21` file field,
# loader 0x53f1ae); design+0x28 = [esp]-0x58 is the price, design+0x2c =
# [esp]-0x5c the waypoint team, design+0x34 = [esp]-0x50 the class tag.
#
# Mode Entry: log every 0x5f4e2a call (vtable, [vtable+0x28], record, args).
# Mode Fn:    log every call of -Fn (the discovered virtual) with ECX and the
#             first stack dwords, to map the record/handle arguments.
#
# The Cpg04 campaign world (row 3) has 71 show-feature waypoints; the model
# calls fire during the park load after the mission Start click, no placement.
#
# Background-safe: virtual input only (DINPUT8 proxy + run-test/dino_input.txt),
# no SetCursorPos / mouse_event / SetForegroundWindow on the game, no shots.

$root = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
. (Join-Path $root "scripts\tools.ps1")
$h = Get-X32dbgHeadless
$wd = Join-Path $root $GameDir
$exe = Join-Path $wd "DinoIsland.exe"
if (-not (Test-Path $exe)) { throw "missing $exe" }
if (-not $Fn -and $Mode -eq 'Fn') { throw "Mode Fn needs -Fn <address>" }
if (-not $Log) {
    $Log = if ($Mode -eq 'Entry') {
        "notes\harness\model-manager-entry.log"
    } else {
        "notes\harness\model-manager-fn.log"
    }
}
$logPath = Join-Path $root $Log
New-Item -ItemType Directory -Force (Split-Path $logPath -Parent) | Out-Null
$dinoDat = Join-Path $wd "save\dino.dat"
$dinoDatBackup = $null

# A stale retail process must never be picked up or killed.
$existing = @(Get-Process DinoIsland -ErrorAction SilentlyContinue)
if ($existing.Count -gt 0) {
    throw ("DinoIsland already running (PID {0}); refusing to kill another lane" -f `
        (($existing | ForEach-Object { $_.Id }) -join ','))
}

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
public class MmWin32 {
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
public class MmOutput {
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
$script:probeLines = @()
$hookLog = Join-Path $wd "dino_hook.log"

Set-Content -Path $logPath -Value ("# model-manager probe mode=$Mode {0}" -f (Get-Date).ToString('s')) -Encoding utf8

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
[MmOutput]::Pump($p.StandardOutput)
$errTask = $p.StandardError.ReadToEndAsync()

function Send([string]$line) {
    $p.StandardInput.WriteLine($line)
    $p.StandardInput.Flush()
}

function Read-NewLines {
    $all = [MmOutput]::Snapshot()
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
            if ($line -match 'MODELMGR|MODELCALL|PARKREADY') { $script:probeLines += $line }
            if ($line -match 'Invalid|ERROR|EXCEPTION') { Write-Host ("debugger: " + $line.Trim()) }
        }
    }
}

function Get-GameProcess {
    # Only the process headless started may receive clicks; a stale
    # DinoIsland.exe from an earlier run must never be picked up instead.
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
    return [MmWin32]::IsWindow($script:gameHwnd)
}

function Wake-GameWindow {
    # Posted messages only; the OS foreground window, z-order and pointer are
    # untouched, so the game keeps running behind the user's window.
    for ($i = 0; $i -lt 8; $i++) {
        if (-not (Update-GameWindow)) { Start-Sleep -Milliseconds 200; continue }
        [MmWin32]::PostMessage($script:gameHwnd, 0x001C, [IntPtr]1, [IntPtr]::Zero) | Out-Null
        [MmWin32]::PostMessage($script:gameHwnd, 0x0006, [IntPtr]1, [IntPtr]::Zero) | Out-Null
        [MmWin32]::PostMessage($script:gameHwnd, 0x0007, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
        [MmWin32]::ShowWindow($script:gameHwnd, 4) | Out-Null
        Start-Sleep -Milliseconds 200
        $c = New-Object MmWin32+RECT
        [MmWin32]::GetClientRect($script:gameHwnd, [ref]$c) | Out-Null
        if (-not [MmWin32]::IsIconic($script:gameHwnd) -and $c.Right -gt 0 -and $c.Bottom -gt 0) {
            return $true
        }
    }
    return $false
}

function Get-InputLogOffset {
    if (-not (Test-Path $hookLog)) { return 0 }
    return (Get-Item $hookLog).Length
}

function Wait-InputDelivered([long]$offset, [int]$timeoutMs) {
    # The proxy appends "virtual mouse: state button=down/up" when the game
    # polls DirectInput and the virtual button was applied. A click that adds
    # no line never reached the game.
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
        Start-Sleep -Milliseconds 500
    }
    return $false
}

# Windows PowerShell 5.1 prefixes the first stdin write with a UTF-8 BOM;
# let a blank line absorb it.
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

        # Probes are only processed while the debuggee is stopped. The show
        # features are created during the world import after the Start click.
        Send 'pause'
        Start-Sleep -Milliseconds 500
        Send 'bp 0x57dc60'
        Send 'SetBreakpointCommand 0x57dc60, "log PARKREADY|fence={p:ecx}; bc 0x57dc60; run"'
        if ($Mode -eq 'Entry') {
            $entry = 'log MODELMGR|mgr={p:ecx}|vt={p:[ecx]}|fn={p:[[ecx]+0x28]}|rec={p:[esp]}' +
                '|disp={d:[[esp]-0x54]}|wteam={d:[[esp]-0x58]}|price={d:[[esp]-0x5c]}' +
                '|tagp={p:[[esp]-0x50]}|tag={s:[[esp]-0x50]}' +
                '|actor={p:[esp+4]}|cls={s:[[[esp+4]+0x28]+0x28]}|scene={p:[esp+8]}' +
                '|self={p:esi}|t={d:GetTickCount()}; run'
            Send 'bp 0x5f4e2a'
            Send ('SetBreakpointCommand 0x5f4e2a, "' + $entry + '"')
        } else {
            $call = 'log MODELCALL|mgr={p:ecx}|vt={p:[ecx]}|rec={p:[esp+4]}' +
                '|disp={d:[[esp+4]-0x54]}|wteam={d:[[esp+4]-0x58]}|price={d:[[esp+4]-0x5c]}' +
                '|tag={s:[[esp+4]-0x50]}' +
                '|actor={p:[esp+8]}|cls={s:[[[esp+8]+0x28]+0x28]}|scene={p:[esp+0xc]}' +
                '|ret={p:[esp]}|t={d:GetTickCount()}; run'
            Send ('bp ' + $Fn)
            Send ('SetBreakpointCommand ' + $Fn + ', "' + $call + '"')
        }
        Send 'run'
        Start-Sleep -Milliseconds 300

        Write-Host "clicking mission Start"
        Click-Client 521 449 | Out-Null

        Write-Host "waiting for the park load / probe hits"
        $deadline = (Get-Date).AddSeconds($ParkTimeoutSec)
        $tick = 0
        while ((Get-Date) -lt $deadline -and $script:probeLines.Count -eq 0) {
            Start-Sleep -Milliseconds 200
            Collect-Log
            $tick++
            if (($tick % 5) -eq 0) { Wake-GameWindow | Out-Null }
        }
        $first = @($script:probeLines | Select-Object -First 3)
        foreach ($line in $first) { Write-Host ("first: " + $line.Trim()) }

        $settle = (Get-Date).AddSeconds($SimSeconds)
        $lastCount = -1
        while ((Get-Date) -lt $settle) {
            Start-Sleep -Milliseconds 250
            Collect-Log
            if ($script:probeLines.Count -eq $lastCount -and (Get-Date).AddSeconds(3) -lt $settle) {
                $settle = (Get-Date).AddSeconds(3)
            }
            $lastCount = $script:probeLines.Count
        }
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
$errPath = "$logPath.err"
$err | Set-Content -Path $errPath -Encoding utf8

Write-Host "--- probe summary ---"
Write-Host ("log: {0}" -f $logPath)
$entries = @(Select-String -Path $logPath -Pattern 'MODELMGR')
$calls = @(Select-String -Path $logPath -Pattern 'MODELCALL')
$ready = @(Select-String -Path $logPath -Pattern 'PARKREADY')
Write-Host ("PARKREADY={0} MODELMGR={1} MODELCALL={2}" -f $ready.Count, $entries.Count, $calls.Count)
$vt = @{}
$fns = @{}
foreach ($e in $entries) {
    if ($e.Line -match 'vt=([0-9A-Fa-f]+)') { $vt[$Matches[1].ToUpper()] = 1 }
    if ($e.Line -match 'fn=([0-9A-Fa-f]+)') { $fns[$Matches[1].ToUpper()] = 1 }
}
Write-Host "--- distinct vt ---"
$vt.Keys | Sort-Object | ForEach-Object { $_ }
Write-Host "--- distinct fn ---"
$fns.Keys | Sort-Object | ForEach-Object { $_ }
Write-Host "--- first 12 probe lines ---"
@($script:probeLines | Select-Object -First 12) | ForEach-Object { $_.Trim() }
