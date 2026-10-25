param(
    [int]$Seconds = 25,
    [string]$Log = "notes\x32dbg-startup.out.log",
    [string]$GameDir = "run"
)

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

Send 'bp CreateFileA'
Send 'SetBreakpointCommand CreateFileA, "log CF_{s:[esp+4]}; run"'
Send 'bp CreateFileW'
Send 'SetBreakpointCommand CreateFileW, "log CW_{s:[esp+4]}; run"'
Send 'bp GetVolumeInformationA'
Send 'SetBreakpointCommand GetVolumeInformationA, "log GVI_ret={p:[esp]}_root={s:[esp+4]}; run"'
Send 'bp GetDriveTypeA'
Send 'SetBreakpointCommand GetDriveTypeA, "log GDT_ret={p:[esp]}_root={s:[esp+4]}; run"'
# The debugger stops at the system breakpoint, then at the entry breakpoint.
# Send run a few times to get past both before waiting.
Send 'run'
Start-Sleep -Milliseconds 700
Send 'run'
Start-Sleep -Milliseconds 700
Send 'run'

$half = [Math]::Max(5, [int]($Seconds / 2))
Start-Sleep -Seconds $half
try {
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
    $bmp.Save((Join-Path $root "notes\game-screen.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
} catch {
    Write-Host "screenshot failed: $_"
}
Start-Sleep -Seconds ($Seconds - $half)

Send 'exit'
if (-not $p.WaitForExit(10000)) {
    $p.Kill()
}
$out = $outTask.Result
$err = $errTask.Result
$out | Set-Content -Path $logPath -Encoding utf8
$err | Set-Content -Path ($logPath -replace '\.out\.', '.err.') -Encoding utf8

Write-Host "wrote $logPath"
Get-Content $logPath -Tail 80
