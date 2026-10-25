param(
    [int]$Port = 8000,
    [switch]$NoWait
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

$listening = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
if ($listening) {
    Write-Host "Ghidra MCP already listening on port $Port (PID $($listening.OwningProcess))."
    exit 0
}

$ghidra = Get-ChildItem "C:\Tools" -Directory -Filter "ghidra_*_PUBLIC" -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | Select-Object -First 1
if (-not $ghidra) {
    throw "Ghidra not found under C:\Tools. Download Ghidra 12.x and extract it there."
}
$env:GHIDRA_INSTALL_DIR = $ghidra.FullName

# Ghidra 12.2 requires JDK 25. Use the portable JDK under C:\Tools if present.
$jdk = Get-ChildItem "C:\Tools\jdk25" -Directory -Filter "jdk-*" -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | Select-Object -First 1
if ($jdk) {
    $env:JAVA_HOME = $jdk.FullName
    $env:PATH = (Join-Path $jdk.FullName "bin") + ";" + $env:PATH
    Write-Host "Using JAVA_HOME=$($jdk.FullName)"
} elseif ((java -version 2>&1 | Select-String -Pattern '"25\.').Count -eq 0) {
    throw "Ghidra 12.2 requires JDK 25. Install a JDK 25 under C:\Tools\jdk25 or set JAVA_HOME."
}

$mcp = Join-Path $root ".venv\Scripts\pyghidra-mcp.exe"
if (-not (Test-Path $mcp)) { throw "pyghidra-mcp not installed in .venv" }

$exe = Join-Path $root "game\%MAINDIR%\DinoIsland.exe"
if (-not (Test-Path $exe)) { throw "DinoIsland.exe not found at $exe" }

$project = Join-Path $root "analysis\ghidra"
$logs = Join-Path $root "analysis\logs"
New-Item -ItemType Directory -Force $project, $logs | Out-Null

$args = @(
    "--transport", "streamable-http",
    "--host", "127.0.0.1",
    "--port", "$Port",
    "--project-path", "`"$project`"",
    "--project-name", "dino"
)
if (-not $NoWait) { $args += "--wait-for-analysis" }
$args += "`"$exe`""

$outLog = Join-Path $logs "ghidra-mcp.out.log"
$errLog = Join-Path $logs "ghidra-mcp.err.log"
$proc = Start-Process -FilePath $mcp -ArgumentList $args -WorkingDirectory $root `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog `
    -WindowStyle Hidden -PassThru

Write-Host "Started pyghidra-mcp (PID $($proc.Id))."
Write-Host "  project: $project"
Write-Host "  log:     $outLog"
Write-Host "Waiting for http://127.0.0.1:$Port/mcp ..."

$deadline = (Get-Date).AddMinutes(15)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 3
    if (Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue) {
        Write-Host "Ghidra MCP is listening on port $Port." -ForegroundColor Green
        exit 0
    }
    if ($proc.HasExited) {
        Write-Host "pyghidra-mcp exited with code $($proc.ExitCode)." -ForegroundColor Red
        Get-Content $errLog -Tail 40 -ErrorAction SilentlyContinue
        exit 1
    }
}

Write-Host "Timed out waiting for the server. Check $errLog" -ForegroundColor Yellow
exit 1
