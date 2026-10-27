param(
    [int]$Seconds = 25,
    [string]$Log = "notes\x32dbg-startup.out.log",
    [string]$GameDir = "",
    [string]$Shot = "notes\harness\startup-screen.png",
    [switch]$PhysicalInput
)

$root = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
. (Join-Path $root "scripts\tools.ps1")
$h = Get-X32dbgHeadless
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
    $shotPath = Join-Path $root $Shot
    New-Item -ItemType Directory -Force (Split-Path $shotPath -Parent) | Out-Null
    & (Join-Path $PSScriptRoot "Capture-Window.ps1") -Path $shotPath
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
