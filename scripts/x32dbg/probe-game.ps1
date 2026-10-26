param(
    [int]$Seconds = 30,
    [string]$GameDir = "",
    [string]$Log = "notes\x32dbg-interact.out.log",
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

if (-not $PhysicalInput -and -not (Test-Path $exe)) {
    & (Join-Path $root "scripts\setup-test-runtime.ps1")
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

Send 'bp CreateFileA'
Send 'SetBreakpointCommand CreateFileA, "log CF_{s:[esp+4]}; run"'
Send 'bp CreateFileW'
Send 'SetBreakpointCommand CreateFileW, "log CW_{s:[esp+4]}; run"'
Send 'run'
Start-Sleep -Milliseconds 700
Send 'run'
Start-Sleep -Milliseconds 700
Send 'run'

Start-Sleep -Seconds 10

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32 {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

$game = Get-Process DinoIsland -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $game) {
    Write-Host "game process not found"
} else {
    Write-Host "game PID=$($game.Id) title='$($game.MainWindowTitle)' hwnd=$($game.MainWindowHandle)"
    if ($game.MainWindowHandle -ne 0) {
        [Win32]::ShowWindow($game.MainWindowHandle, 9) | Out-Null
        [Win32]::SetForegroundWindow($game.MainWindowHandle) | Out-Null
        Start-Sleep -Milliseconds 500

        Add-Type -AssemblyName System.Drawing
        $rect = New-Object Win32+RECT
        [Win32]::GetWindowRect($game.MainWindowHandle, [ref]$rect) | Out-Null
        $w = $rect.Right - $rect.Left
        $hgt = $rect.Bottom - $rect.Top
        if ($w -gt 0 -and $hgt -gt 0) {
            $bmp = New-Object System.Drawing.Bitmap $w, $hgt
            $g = [System.Drawing.Graphics]::FromImage($bmp)
            $hdc = $g.GetHdc()
            [Win32]::PrintWindow($game.MainWindowHandle, $hdc, 0) | Out-Null
            $g.ReleaseHdc($hdc)
            $g.Dispose()
            $shotDir = Join-Path $root "notes\harness"
            New-Item -ItemType Directory -Force $shotDir | Out-Null
            $bmp.Save((Join-Path $shotDir "game-window-before.png"), [System.Drawing.Imaging.ImageFormat]::Png)
            $bmp.Dispose()
        }

        # Try to advance the menu/intro.
        Add-Type -AssemblyName System.Windows.Forms
        Add-Type -AssemblyName Microsoft.VisualBasic
        [Microsoft.VisualBasic.Interaction]::AppActivate($game.Id) | Out-Null
        Start-Sleep -Milliseconds 300
        [System.Windows.Forms.SendKeys]::SendWait("{ENTER}")
        Start-Sleep -Seconds 3
        [System.Windows.Forms.SendKeys]::SendWait("{ENTER}")
        Start-Sleep -Seconds 3
        [System.Windows.Forms.SendKeys]::SendWait(" ")
    }
}

Start-Sleep -Seconds ([Math]::Max(2, $Seconds - 16))

Send 'exit'
if (-not $p.WaitForExit(10000)) { $p.Kill() }
$out = $outTask.Result
$err = $errTask.Result
$out | Set-Content -Path $logPath -Encoding utf8
$err | Set-Content -Path ($logPath -replace '\.out\.', '.err.') -Encoding utf8

Write-Host "--- matches ---"
Select-String -Path $logPath -Pattern "\.skel|\.skin|\.mesh|\.anim|\.tga|Begin3D|EXCEPTION|CreateFile" | Select-Object -Last 80
