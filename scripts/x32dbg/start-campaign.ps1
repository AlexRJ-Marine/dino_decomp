param(
    [int]$Seconds = 60,
    [int]$CampaignRow = 3,
    [int]$MissionRow = 0,
    [switch]$NoTrace,
    [string]$GameDir = "run",
    [string]$Log = "notes\x32dbg-campaign.out.log"
)

# Start a campaign mission through the retail menu and log:
#   - stFence ctor 0x57dc60: rectangle, cell-size pointer and value
#   - saDinosaur::Update 0x571540: action id (+0x2c0) and +0x1e0
#   - asItemBehavior::RegisterForAction 0x56c0c0: vtable and args
#   - stFence::CanAddActor 0x57eb50: occupant, cost index, capacity/used
#
# Cpg04 (campaign row 3) has 9 fences and 20 dinosaurs.

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

Send 'bc'

# Trace breakpoints are installed after the campaign/mission selection page,
# just before Start. Installing them earlier pauses menu loading and makes the
# click sequence timing-dependent.

Send 'run'
Start-Sleep -Milliseconds 700
Send 'run'
Start-Sleep -Milliseconds 700
Send 'run'

Start-Sleep -Seconds 28
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
        $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size $w, $hgt))
        $g.Dispose()
    }
    $bmp.Save((Join-Path $root "notes\$name.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

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

$game = Get-Process DinoIsland -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $game) {
    Write-Host "game process not found"
} else {
    $hwnd = $game.MainWindowHandle
    if ($hwnd -eq 0) {
        Write-Host "no main window handle"
    } else {
        [Microsoft.VisualBasic.Interaction]::AppActivate($game.Id) | Out-Null
        function Resume() {
            Send 'run'
            Start-Sleep -Milliseconds 250
        }
        Resume
        Save-Window $hwnd "campaign-main"
        Write-Host "clicking Campaigns"
        Resume
        Click-Client $hwnd 395 189
        Start-Sleep -Seconds 2
        Save-Window $hwnd "campaign-list"

        Write-Host "selecting campaign row $CampaignRow"
        $campaignY = 166 + $CampaignRow * 34 + 17
        Resume
        Click-Client $hwnd 404 $campaignY
        Start-Sleep -Seconds 1
        Save-Window $hwnd "campaign-selected"

        Write-Host "clicking campaign Start"
        Resume
        Click-Client $hwnd 365 530
        Start-Sleep -Seconds 2
        Save-Window $hwnd "campaign-info"

        Write-Host "selecting mission row $MissionRow"
        $missionY = 142 + $MissionRow * 17 + 8
        Resume
        Click-Client $hwnd 259 $missionY
        Start-Sleep -Seconds 1

        # The fence ctor runs during the world load after Start; install it
        # before the click. The dinosaur/capacity breakpoints are installed
        # only after the briefing has been dismissed.
        if (-not $NoTrace) {
            Send 'bp 0x57dc60'
            Send 'SetBreakpointCommand 0x57dc60, "log FENCE this={p:ecx} p0={p:[esp+4]} p1={p:[esp+8]} cellptr={p:[esp+0xc]} cell={p:[[esp+0xc]]} x0={p:[[esp+4]]} y0={p:[[esp+4]+4]} x1={p:[[esp+8]]} y1={p:[[esp+8]+4]}; run"'
            # Catch the load-time dinosaur -> paddock placement, if it uses
            # the retail capacity gate.
            Send 'bp 0x57eb50'
            Send 'SetBreakpointCommand 0x57eb50, "log CANADD fence={p:ecx} dino={p:[esp+4]} idx={p:[[esp+4]+0x1e0]} cap={p:[ecx+0x7c]} used={p:[ecx+0x80]}; run"'
        }
        Resume

        Write-Host "clicking Start"
        Click-Client $hwnd 521 449
        Start-Sleep -Seconds 6
        Save-Window $hwnd "campaign-loading"
        Start-Sleep -Seconds 22
        Save-Window $hwnd "campaign-park"

        # Mission briefing pages (1/4 ...): the green check closes/advances.
        for ($i = 0; $i -lt 5; $i++) {
            Resume
            Click-Client $hwnd 322 402
            Start-Sleep -Milliseconds 900
        }
        Start-Sleep -Seconds 5
        Save-Window $hwnd "campaign-running"

        # Now that the briefing is gone, trace the live dinosaur action path.
        # 0x56c670 is the "can't reach item" branch; EBP is the saDinosaur
        # ([ebp+0x28] is the printed dino id).
        if (-not $NoTrace) {
            Send 'bp 0x56c670'
            Send 'SetBreakpointCommand 0x56c670, "log ACTOR dino={p:ebp} id={p:[ebp+0x28]} f1e0={p:[ebp+0x1e0]} f1dc={p:[ebp+0x1dc]} f1e4={p:[ebp+0x1e4]} act={p:[ebp+0x2c0]}; run"'
            Send 'bp 0x57eb50'
            Send 'SetBreakpointCommand 0x57eb50, "log CANADD fence={p:ecx} dino={p:[esp+4]} idx={p:[[esp+4]+0x1e0]} cap={p:[ecx+0x7c]} used={p:[ecx+0x80]}; run"'
            Resume
            Start-Sleep -Seconds 10
            Save-Window $hwnd "campaign-sim"
        }
    }
}

Start-Sleep -Seconds ([Math]::Max(2, $Seconds - 40))
Send 'exit'
if (-not $p.WaitForExit(15000)) { $p.Kill() }
$out = $outTask.Result
$err = $errTask.Result
$out | Set-Content -Path $logPath -Encoding utf8
$err | Set-Content -Path ($logPath -replace '\.out\.', '.err.') -Encoding utf8

Write-Host "--- campaign trace matches ---"
Select-String -Path $logPath -Pattern "FENCE |DINO |CLIPPAIR|CANADD|EXCEPTION" | Select-Object -First 120
