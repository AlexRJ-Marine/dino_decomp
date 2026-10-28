param(
    [int]$Seconds = 60,
    [int]$CampaignRow = 3,
    [int]$MissionRow = 0,
    [switch]$NoTrace,
    [switch]$GenotypeTrace,
    [switch]$ProviderTrace,
    [switch]$UnlockCampaigns,
    [switch]$BuyDino,
    [switch]$SecuTrace,
    [switch]$FileTrace,
    [switch]$PhysicalInput,
    [switch]$VirtualMouse,
    [int]$ParkTimeoutSec = 120,
    [string]$GameDir = "",
    [string]$Log = "notes\x32dbg-campaign.out.log",
    [string]$ShotDir = "notes\harness"
)

# Start a campaign mission through the retail menu and log:
#   - stFence ctor 0x57dc60: rectangle, cell-size pointer and value
#   - saDinosaur::Update 0x571540: action id (+0x2c0) and +0x1e0
#   - asItemBehavior::RegisterForAction 0x56c0c0: vtable and args
#   - stFence::CanAddActor 0x57eb50: occupant, cost index, capacity/used
#   - -SecuTrace: danger-grid stamps 0x571b50 and the post-store SECU at
#     0x5714a5 (validate with tools/recon/secu_grid_check.py)
#
# Cpg04 (campaign row 3) has 9 fences and 20 dinosaurs.
# -UnlockCampaigns backs up run/save/dino.dat, writes 0F (campaigns 1-4) and
# restores it after the run; without it the campaign must already be unlocked.
#
# Background safety: the retail app stops its frame loop and DirectInput
# polling once it is no longer the active window; its WndProc resumes both only
# after WM_ACTIVATEAPP/WM_ACTIVATE/WM_SETFOCUS (0x4235b0 flips the flags at
# app+0x54 and +0x31cec). The script posts those messages and restores the
# window with SW_SHOWNOACTIVATE: no real activation, no SetForegroundWindow on
# the game, no pointer movement. Each click is verified through
# run-test/dino_hook.log ("state button="), so a click that never reached the
# game is reported instead of silently lost.
#
# Screenshots use PrintWindow and crop the 800x600 client area. They carry the
# real frame while the game is drawing; a minimized/idle window returns a blank
# client area, so blank shots are reported and not written (a blank PNG is not
# evidence). A live breakpoint line (FENCE/DINORESET, or PARKREADY with
# -NoTrace) is the "park loaded" signal, never the screenshot.

$root = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
. (Join-Path $root "scripts\tools.ps1")
$h = Get-X32dbgHeadless
if (-not $GameDir) {
    $GameDir = if ($PhysicalInput) { "run" } else { "run-test" }
}
$wd = Join-Path $root $GameDir
$exe = Join-Path $wd "DinoIsland.exe"
$logPath = Join-Path $root $Log
$dinoDat = Join-Path $wd "save\dino.dat"
$dinoDatBackup = $null
if (-not $PhysicalInput) {
    $VirtualMouse = $true
}
$previousForeground = [IntPtr]::Zero
if ($VirtualMouse) {
    if (-not (Test-Path $exe)) {
        & (Join-Path $root "scripts\setup-test-runtime.ps1")
    }
    $env:DINO_VIRTUAL_MOUSE = "1"
    # Retail stops polling DirectInput when it is not the active window; the
    # proxy reports active when this is set (same contract as the target trace).
    $env:DINO_FORCE_INPUT_ACTIVE = "1"
    . (Join-Path $PSScriptRoot "VirtualInput.ps1")
    $previousForeground = [DinoVirtualInputShared]::GetForegroundWindow()
}
if ($UnlockCampaigns -and (Test-Path $dinoDat)) {
    $dinoDatBackup = [System.IO.File]::ReadAllBytes($dinoDat)
    [System.IO.File]::WriteAllBytes($dinoDat, [byte[]](0x0F, 0, 0, 0))
    Write-Host "unlocked campaigns 1-4 (dino.dat backed up)"
}

Add-Type @"
using System;
using System.Collections.Concurrent;
using System.Runtime.InteropServices;
public class Win32 {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
public class DinoHarnessOutput {
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
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName Microsoft.VisualBasic
Add-Type -AssemblyName System.Windows.Forms

# The debugger log is drained line by line so the script can wait for a live
# trace line (breakpoint hit) instead of guessing from a screenshot.
$script:gameHwnd = [IntPtr]::Zero
$script:traceSeen = 0
$hookLog = Join-Path $wd "dino_hook.log"

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
[DinoHarnessOutput]::Pump($p.StandardOutput)
$errTask = $p.StandardError.ReadToEndAsync()

function Send([string]$line) {
    $p.StandardInput.WriteLine($line)
    $p.StandardInput.Flush()
}

# Windows PowerShell 5.1 prefixes the first stdin write with a UTF-8 BOM,
# which x64dbg rejects as part of the command; let a blank line absorb it.
Send ''
Send 'bc'

# Trace breakpoints are installed after the campaign/mission selection page,
# just before Start. Installing them earlier pauses menu loading and makes the
# click sequence timing-dependent.

Send 'run'
Start-Sleep -Milliseconds 700
Send 'run'
Start-Sleep -Milliseconds 700
Send 'run'

Start-Sleep -Seconds 5

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
    $h = $proc.MainWindowHandle
    if ($h -ne [IntPtr]::Zero -and $h -ne $script:gameHwnd) {
        if ($script:gameHwnd -ne [IntPtr]::Zero) {
            Write-Host ("game window changed: 0x{0:X} -> 0x{1:X}" -f `
                $script:gameHwnd.ToInt64(), $h.ToInt64())
        }
        $script:gameHwnd = $h
    }
    return [Win32]::IsWindow($script:gameHwnd)
}

function Get-WindowState {
    if (-not [Win32]::IsWindow($script:gameHwnd)) { return "window=invalid" }
    $r = New-Object Win32+RECT
    $c = New-Object Win32+RECT
    [Win32]::GetWindowRect($script:gameHwnd, [ref]$r) | Out-Null
    [Win32]::GetClientRect($script:gameHwnd, [ref]$c) | Out-Null
    return ("hwnd=0x{0:X} rect={1},{2},{3},{4} client={5}x{6} iconic={7}" -f `
        $script:gameHwnd.ToInt64(), $r.Left, $r.Top, $r.Right, $r.Bottom,
        ($c.Right - $c.Left), ($c.Bottom - $c.Top), [Win32]::IsIconic($script:gameHwnd))
}

function Wake-GameWindow {
    # The retail app stops its frame loop (and DirectInput polling) once it is
    # no longer the active window, and its WndProc only flips the active flags
    # on WM_ACTIVATEAPP/WM_ACTIVATE/WM_SETFOCUS (0x4235b0; +0x54 and +0x31cec).
    # These are posted messages only: the OS foreground window, the z-order
    # and the pointer are untouched, so the game keeps running in the
    # background while the user works in another window.
    for ($i = 0; $i -lt 8; $i++) {
        if (-not (Update-GameWindow)) { Start-Sleep -Milliseconds 200; continue }
        [Win32]::PostMessage($script:gameHwnd, 0x001C, [IntPtr]1, [IntPtr]::Zero) | Out-Null
        [Win32]::PostMessage($script:gameHwnd, 0x0006, [IntPtr]1, [IntPtr]::Zero) | Out-Null
        [Win32]::PostMessage($script:gameHwnd, 0x0007, [IntPtr]::Zero,
                             [IntPtr]::Zero) | Out-Null
        [Win32]::ShowWindow($script:gameHwnd, 4) | Out-Null
        Start-Sleep -Milliseconds 200
        $c = New-Object Win32+RECT
        [Win32]::GetClientRect($script:gameHwnd, [ref]$c) | Out-Null
        if (-not [Win32]::IsIconic($script:gameHwnd) -and
            $c.Right -gt 0 -and $c.Bottom -gt 0) {
            return $true
        }
    }
    return $false
}

function Save-Window([string]$name) {
    if (-not (Wake-GameWindow)) {
        Write-Host "shot $name : $(Get-WindowState)"
        return
    }
    if (-not $VirtualMouse) {
        [Win32]::ShowWindow($script:gameHwnd, 9) | Out-Null
        [Win32]::SetForegroundWindow($script:gameHwnd) | Out-Null
        Start-Sleep -Milliseconds 200
    }
    $r = New-Object Win32+RECT
    $c = New-Object Win32+RECT
    [Win32]::GetWindowRect($script:gameHwnd, [ref]$r) | Out-Null
    [Win32]::GetClientRect($script:gameHwnd, [ref]$c) | Out-Null
    $w = $r.Right - $r.Left
    $hgt = $r.Bottom - $r.Top
    $cw = $c.Right - $c.Left
    $ch = $c.Bottom - $c.Top
    if ($w -le 0 -or $hgt -le 0 -or $cw -le 0 -or $ch -le 0) {
        Write-Host "shot $name : $(Get-WindowState)"
        return
    }
    $origin = New-Object Win32+POINT
    [Win32]::ClientToScreen($script:gameHwnd, [ref]$origin) | Out-Null
    $full = New-Object System.Drawing.Bitmap $w, $hgt
    $g = [System.Drawing.Graphics]::FromImage($full)
    try {
        $hdc = $g.GetHdc()
        [Win32]::PrintWindow($script:gameHwnd, $hdc, 0) | Out-Null
        $g.ReleaseHdc($hdc)
    } finally {
        $g.Dispose()
    }
    # Crop the client area: the window frame is not evidence.
    $cropRect = New-Object System.Drawing.Rectangle ($origin.X - $r.Left), ($origin.Y - $r.Top), $cw, $ch
    $bmp = $full.Clone($cropRect, $full.PixelFormat)
    $full.Dispose()
    # PrintWindow sees the client area only while the game is drawing; a
    # minimized or idle window yields a uniform white client. Only save a shot
    # that actually carries pixels; a blank PNG is not evidence.
    $colors = @{}
    $sampled = 0
    for ($y = 0; $y -lt $ch; $y += 16) {
        for ($x = 0; $x -lt $cw; $x += 16) {
            $p = $bmp.GetPixel($x, $y)
            $sampled++
            if (-not ($p.R -ge 250 -and $p.G -ge 250 -and $p.B -ge 250)) {
                $colors["$($p.R),$($p.G),$($p.B)"] = 1
            }
        }
    }
    if ($sampled -eq 0 -or $colors.Count -lt 4) {
        Write-Host (("shot {0}: blank {1}x{2} client (PrintWindow cannot capture the " +
                     "dgVoodoo D3D window while the game is in the background; not saved); {3}") -f `
                     $name, $cw, $ch, (Get-WindowState))
        $bmp.Dispose()
        return
    }
    $dir = Join-Path $root $ShotDir
    New-Item -ItemType Directory -Force $dir | Out-Null
    $path = Join-Path $dir "$name.png"
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host ("shot {0}: {1}x{2} -> {3}" -f $name, $cw, $ch, $path)
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
        try {
            $fs = [System.IO.File]::Open($hookLog, 'Open', 'Read', 'ReadWrite')
        } catch {
            continue
        }
        try {
            if ($fs.Length -le $offset) { continue }
            $fs.Position = $offset
            $reader = New-Object System.IO.StreamReader($fs)
            if (($reader.ReadToEnd()) -match 'state button=') { return $true }
        } finally {
            $fs.Dispose()
        }
    }
    return $false
}

function Click-Client([int]$x, [int]$y) {
    if (-not $VirtualMouse) {
        [Win32]::ShowWindow($script:gameHwnd, 9) | Out-Null
        [Win32]::SetForegroundWindow($script:gameHwnd) | Out-Null
        Start-Sleep -Milliseconds 300
        $r = New-Object Win32+RECT
        [Win32]::GetWindowRect($script:gameHwnd, [ref]$r) | Out-Null
        [Win32]::SetCursorPos($r.Left + $x, $r.Top + $y) | Out-Null
        Start-Sleep -Milliseconds 150
        [Win32]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 80
        [Win32]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 300
        return
    }
    for ($attempt = 1; $attempt -le 2; $attempt++) {
        if (-not (Wake-GameWindow)) {
            Write-Host "click $x,$y : window not restorable ($(Get-WindowState))"
        }
        $offset = Get-InputLogOffset
        Invoke-VirtualClick -Hwnd $script:gameHwnd -X $x -Y $y -GameDir $wd
        if (Wait-InputDelivered -Offset $offset -TimeoutMs 1500) { return }
        Write-Host ("click {0},{1} not delivered (attempt {2}); {3}" -f `
            $x, $y, $attempt, (Get-WindowState))
        Start-Sleep -Milliseconds 500
    }
}

function Read-NewTraceLines {
    $all = [DinoHarnessOutput]::Snapshot()
    if ($all.Count -le $script:traceSeen) { return @() }
    $new = @($all[$script:traceSeen..($all.Count - 1)])
    $script:traceSeen = $all.Count
    return $new
}

function Wait-TraceMatch([string[]]$patterns, [int]$timeoutSec, [string]$label) {
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    $tick = 0
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
        $tick++
        foreach ($line in (Read-NewTraceLines)) {
            foreach ($pattern in $patterns) {
                if ($line.Contains($pattern)) {
                    Write-Host ("{0}: {1}" -f $label, $line.Trim())
                    return $true
                }
            }
        }
        # The engine re-checks activation and minimizes itself when it thinks it
        # is inactive; keep it awake while waiting for a trace line.
        if (($tick % 4) -eq 0) { Wake-GameWindow | Out-Null }
        Send 'run'
    }
    Write-Host ("{0}: timed out after {1}s ({2})" -f $label, $timeoutSec, (Get-WindowState))
    return $false
}

$game = Get-GameProcess
if (-not $game) {
    Write-Host "game process not found"
} else {
    if (-not (Update-GameWindow)) {
        Write-Host "no main window handle"
    } else {
        if ($VirtualMouse) {
            & (Join-Path $root "scripts\Set-WindowedStyle.ps1") -Hwnd $script:gameHwnd -SendToBack
            Restore-Foreground $previousForeground
        } else {
            [Microsoft.VisualBasic.Interaction]::AppActivate($game.Id) | Out-Null
            [Win32]::ShowWindow($script:gameHwnd, 9) | Out-Null
            [Win32]::SetForegroundWindow($script:gameHwnd) | Out-Null
        }
        Start-Sleep -Milliseconds 400
        Write-Host ("game pid={0} {1}" -f $game.Id, (Get-WindowState))
        # Losing activation makes the retail app minimize itself and stop
        # polling DirectInput; restore it without activation and post
        # WM_SETFOCUS so its frame loop runs again.
        if (-not (Wake-GameWindow)) {
            Write-Host "game window did not restore without activation"
        }
        Start-Sleep -Seconds 2
        Write-Host ("waiting for the menu to render: " + (Get-WindowState))
        function Resume() {
            Send 'run'
            Start-Sleep -Milliseconds 250
        }
        Resume
        Save-Window "campaign-main"

        # Genotype/creation loggers must be armed while the debuggee is
        # stopped: commands sent while it runs are queued and only processed
        # at the next break, which is after the world load has finished.
        if ($GenotypeTrace) {
            Send 'pause'
            Start-Sleep -Milliseconds 500
            Send 'bp 0x586520'
            Send 'SetBreakpointCommand 0x586520, "log GEN obj={p:[esp+8]} g0a={p:[[esp+4]]} g0b={p:[[esp+4]+4]} g1a={p:[[esp+4]+0xc]} g1b={p:[[esp+4]+0x10]} g2a={p:[[esp+4]+0x18]} g2b={p:[[esp+4]+0x1c]} g3a={p:[[esp+4]+0x24]} g3b={p:[[esp+4]+0x28]}; run"'
            # The actor ctor is the common funnel for every creation path; log
            # the caller and the runtime genotype pointer (arg 2) there.
            Send 'bp 0x56fed0'
            Send 'SetBreakpointCommand 0x56fed0, "log CTOR ret={p:[esp]} gen={p:[esp+8]} g0a={p:[[esp+8]]} g0b={p:[[esp+8]+4]} g1a={p:[[esp+8]+0xc]} g1b={p:[[esp+8]+0x10]} g2a={p:[[esp+8]+0x18]} g2b={p:[[esp+8]+0x1c]} g3a={p:[[esp+8]+0x24]} g3b={p:[[esp+8]+0x28]} p98={p:[[esp+4]+0x98]} pa4={p:[[esp+4]+0xa4]} pa8={p:[[esp+4]+0xa8]} cat={p:[[esp+4]+0x190]} p0={p:[[esp+4]]} pc={p:[[esp+4]+0xc]} p14={p:[[esp+4]+0x14]} p18={p:[[esp+4]+0x18]} p1c={p:[[esp+4]+0x1c]} p28={p:[[esp+4]+0x28]} p2c={p:[[esp+4]+0x2c]} p34={p:[[esp+4]+0x34]} p38={p:[[esp+4]+0x38]} p3c={p:[[esp+4]+0x3c]} p40={p:[[esp+4]+0x40]} p48={p:[[esp+4]+0x48]} p4c={p:[[esp+4]+0x4c]} p60={p:[[esp+4]+0x60]} p68={p:[[esp+4]+0x68]} p6c={p:[[esp+4]+0x6c]} p70={p:[[esp+4]+0x70]} p74={p:[[esp+4]+0x74]} p1b4={p:[[esp+4]+0x1b4]}; run"'
            # Dump the runtime Part 3 Length cells for the hybrid pairs used by
            # Cpg04: (0,4) and (1,4) and (18,0), for Head/Body/Tail.
            Send 'bp 0x619830'
            Send 'SetBreakpointCommand 0x619830, "log TBL h04={p:[[[[ecx+0x308]]+0x10]]} h14={p:[[[[ecx+0x308]]+0x10]+4]} h180={p:[[[[ecx+0x308]]]+0x48]} b04={p:[[[[ecx+0x314]]+0x10]]} b00={p:[[[[ecx+0x314]]]]} t04={p:[[[[ecx+0x318]]+0x10]]} t14={p:[[[[ecx+0x318]]+0x10]+4]} t180={p:[[[[ecx+0x318]]]+0x48]}; run"'
            Send 'bp 0x570fd0'
            Send 'SetBreakpointCommand 0x570fd0, "log DINORESET dino={p:ecx} id={p:[ecx+0x28]} f1e0={p:[ecx+0x1e0]} len={p:[ecx+0x50]} p98={p:[ecx+0xe8]} pa4={p:[ecx+0xf4]} pa8={p:[ecx+0xf8]} f1b4={p:[ecx+0x204]}; run"'
            Send 'run'
            Start-Sleep -Milliseconds 500
        }

        Write-Host "clicking Campaigns"
        Resume
        Click-Client 395 189
        Start-Sleep -Seconds 2
        Save-Window "campaign-list"

        Write-Host "selecting campaign row $CampaignRow"
        $campaignY = 166 + $CampaignRow * 34 + 17
        Resume
        Click-Client 404 $campaignY
        Start-Sleep -Seconds 1
        Save-Window "campaign-selected"

        Write-Host "clicking campaign Start"
        Resume
        Click-Client 365 530
        Start-Sleep -Seconds 2
        Save-Window "campaign-info"

        Write-Host "selecting mission row $MissionRow"
        $missionY = 142 + $MissionRow * 17 + 8
        Resume
        Click-Client 259 $missionY
        Start-Sleep -Seconds 1

        # The fence ctor runs during the world load after Start; install it
        # before the click. The dinosaur/capacity breakpoints are installed
        # only after the briefing has been dismissed.
        if (-not $NoTrace) {
            # Commands are only processed while the debuggee is stopped.
            Send 'pause'
            Start-Sleep -Milliseconds 400
            Send 'bp 0x57dc60'
            Send 'SetBreakpointCommand 0x57dc60, "log FENCE this={p:ecx} p0={p:[esp+4]} p1={p:[esp+8]} cellptr={p:[esp+0xc]} cell={p:[[esp+0xc]]} x0={p:[[esp+4]]} y0={p:[[esp+4]+4]} x1={p:[[esp+8]]} y1={p:[[esp+8]+4]}; run"'
            # Catch the load-time dinosaur -> paddock placement, if it uses
            # the retail capacity gate.
            Send 'bp 0x57eb50'
            Send 'SetBreakpointCommand 0x57eb50, "log CANADD fence={p:ecx} dino={p:[esp+4]} idx={p:[[esp+4]+0x1e0]} cap={p:[ecx+0x7c]} used={p:[ecx+0x80]}; run"'
            # Dinosaur creation/reset: this is the saDinosaur object, so the
            # +0x1e0 cost-index field can be read at load time.
            Send 'bp 0x570fd0'
            Send 'SetBreakpointCommand 0x570fd0, "log DINORESET dino={p:ecx} id={p:[ecx+0x28]} f1e0={p:[ecx+0x1e0]} len={p:[ecx+0x50]} p98={p:[ecx+0xe8]} pa4={p:[ecx+0xf4]} pa8={p:[ecx+0xf8]} f1b4={p:[ecx+0x204]} f1dc={p:[ecx+0x1dc]} act={p:[ecx+0x2c0]}; run"'
            # Live 18-gene block at spawn (first stack arg = actor+0x3c).
            Send 'bp 0x586520'
            Send 'SetBreakpointCommand 0x586520, "log GEN g={p:[esp+4]} g0a={p:[[esp+4]]} g0b={p:[[esp+4]+4]} g1a={p:[[esp+4]+0xc]} g1b={p:[[esp+4]+0x10]} g2a={p:[[esp+4]+0x18]} g2b={p:[[esp+4]+0x1c]} g3a={p:[[esp+4]+0x24]} g3b={p:[[esp+4]+0x28]}; run"'
        } else {
            # -NoTrace still needs proof that the mission Start click loaded the
            # park: one one-shot sentinel in the world import, not a trace dump.
            Send 'pause'
            Start-Sleep -Milliseconds 400
            Send 'bp 0x57dc60'
            Send 'SetBreakpointCommand 0x57dc60, "log PARKREADY fence={p:ecx}; bc 0x57dc60; run"'
        }
        if ($FileTrace) {
            # Every file the park load opens (CF_ = CreateFileA path), e.g.
            # to find which textures draw roads (notes/road-construction-recon.md).
            Send 'pause'
            Start-Sleep -Milliseconds 400
            Send 'bp CreateFileA'
            Send 'SetBreakpointCommand CreateFileA, "log CF_{s:[esp+4]}; run"'
        }
        Resume

        Write-Host "clicking Start"
        Click-Client 521 449
        Start-Sleep -Seconds 4
        Save-Window "campaign-loading"
        Write-Host "waiting for the park to load"
        # x64dbg 2026.x drops the spaces from `log` text ("PARKREADYfence="),
        # so match the marker words alone.
        $parkPatterns = @('PARKREADY')
        if (-not $NoTrace) { $parkPatterns = @('FENCE', 'DINORESET') }
        if (Wait-TraceMatch $parkPatterns $ParkTimeoutSec 'park') {
            # The world import finishes before the briefing panel opens.
            Start-Sleep -Seconds 4
        } else {
            Write-Host ("park not reached: the clicks reached DirectInput " +
                        "(run-test/dino_hook.log) but the retail menu did not act on them " +
                        "while the game is in the background")
        }
        Save-Window "campaign-park"

        # Mission briefing pages (1/4 ...): the green check closes/advances.
        for ($i = 0; $i -lt 5; $i++) {
            Resume
            Click-Client 322 402
            Start-Sleep -Milliseconds 900
        }
        Start-Sleep -Seconds 5
        Save-Window "campaign-running"

        # Now that the briefing is gone, trace the live dinosaur action path.
        # 0x56e670 is the action selector; [ecx+0x28] is the saDinosaur.
        if (-not $NoTrace) {
            Send 'pause'
            Start-Sleep -Milliseconds 400
            Send 'bp 0x56e670'
            Send 'SetBreakpointCommand 0x56e670, "log ACTOR beh={p:ecx} actor={p:[ecx+0x28]} x={p:[[ecx+0x28]+8]} y={p:[[ecx+0x28]+0xc]} f1e0={p:[[ecx+0x28]+0x1e0]} f1dc={p:[[ecx+0x28]+0x1dc]} act={p:[[ecx+0x28]+0x2c0]} tr={p:[[ecx+0x28]+0x2a0]} r={p:[[ecx+0x28]+0x27c]} secu={p:[[ecx+0x28]+0x24c]}; run"'
            if ($ProviderTrace) {
                # FUN_0056e410 calls the action target provider at node+0x3c:
                #   MOV ECX,[ESI+0x3c]; MOV EAX,[ECX]; CALL [EAX+0x14]
                # ECX=provider, EAX=vtable, ESI=node.
                Send 'bp 0x56e455'
                Send 'SetBreakpointCommand 0x56e455, "log PROVIDER obj={p:ecx} vt={p:[ecx]} fn={p:[[ecx]+0x14]} r18={p:[[ecx]+0x18]} r1c={p:[[ecx]+0x1c]} node={p:esi} action={p:[esi+0x38]} target={p:[esi+0x3c]}; bc 0x56e455; run"'
            }
            # Danger-grid validation: one pass of 20 stamps immediately before
            # the 20 per-dinosaur SECU recomputes.
            if ($SecuTrace) {
                Send 'bp 0x571b50'
                Send 'SetBreakpointCommand 0x571b50, "log STAMP actor={p:ecx} x={p:[ecx+8]} y={p:[ecx+0xc]} tr={p:[ecx+0x2a0]} r={p:[ecx+0x27c]}; run"'
                # After the SECU store at 0x57149f, before the epilogue.
                Send 'bp 0x5714a5'
                Send 'SetBreakpointCommand 0x5714a5, "log SECU actor={p:esi} secu={p:[esi+0x24c]} p14={p:[esi+0x14]} p1d4={p:[esi+0x1d4]}; run"'
            }
            Send 'bp 0x57eb50'
            Send 'SetBreakpointCommand 0x57eb50, "log CANADD fence={p:ecx} dino={p:[esp+4]} idx={p:[[esp+4]+0x1e0]} cap={p:[ecx+0x7c]} used={p:[ecx+0x80]}; run"'
            # FENCE_INSERTION state and its 64-byte-record loop.
            Send 'bp 0x535be0'
            Send 'SetBreakpointCommand 0x535be0, "log FENCEINS this={p:ecx} tool={p:[ecx+0x44]} rec={p:[ecx+0x4c]} flags={p:[[ecx+0x44]+0x3c]}/{p:[[ecx+0x44]+0x3d]}; run"'
            Send 'bp 0x535cb6'
            Send 'SetBreakpointCommand 0x535cb6, "log PADVEC this={p:ecx} vec={p:ebp} begin={p:[ebp+4]} end={p:[ebp+8]} idx={p:edi} rec0_20={p:[[ebp+4]+0x20]} rec0_24={p:[[ebp+4]+0x24]} rec0_2c={p:[[ebp+4]+0x2c]}; run"'
            # stFence::AddActor.
            Send 'bp 0x57e8a0'
            Send 'SetBreakpointCommand 0x57e8a0, "log ADDACTOR fence={p:ecx} dino={p:[esp+4]} idx={p:[[esp+4]+0x1e0]} cap={p:[ecx+0x7c]} used={p:[ecx+0x80]}; run"'
            Resume
            Start-Sleep -Seconds 20
            Save-Window "campaign-sim"
        }

        if ($BuyDino) {
            Write-Host "waiting for the park before the store"
            Start-Sleep -Seconds 2
            Resume
            Write-Host "opening the dinosaur store"
            Click-Client 765 298
            Start-Sleep -Seconds 2
            Save-Window "dino-store"
            Write-Host "selecting store row 0"
            Click-Client 168 76
            Start-Sleep -Seconds 1
            Save-Window "dino-store-selected"
            Write-Host "buying the dinosaur"
            Click-Client 471 521
            Start-Sleep -Seconds 2
            Save-Window "dino-bought"
            Write-Host "placing the dinosaur in a paddock"
            foreach ($pt in @(@(200,220), @(600,220), @(250,300), @(550,300), @(400,200), @(400,400))) {
                Resume
                Click-Client $pt[0] $pt[1]
                Start-Sleep -Seconds 3
            }
            Start-Sleep -Seconds 2
            Save-Window "dino-placed"
            Start-Sleep -Seconds 3
        }
    }
}

Start-Sleep -Seconds ([Math]::Max(2, $Seconds - 40))
Send 'exit'
if (-not $p.WaitForExit(15000)) { $p.Kill() }
# Let the log pump drain the remaining stdout.
Start-Sleep -Milliseconds 500
if ($null -ne $dinoDatBackup) {
    [System.IO.File]::WriteAllBytes($dinoDat, $dinoDatBackup)
    Write-Host "restored dino.dat"
}
$out = [DinoHarnessOutput]::Snapshot()
$err = $errTask.Result
$out | Set-Content -Path $logPath -Encoding utf8
$err | Set-Content -Path ($logPath -replace '\.out\.', '.err.') -Encoding utf8

Write-Host "--- campaign trace matches ---"
Select-String -Path $logPath -Pattern "FENCE |DINORESET|ACTOR |CANADD|PROVIDER|PARKREADY|EXCEPTION" | Select-Object -First 120
