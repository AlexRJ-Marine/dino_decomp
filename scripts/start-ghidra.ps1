param(
    [int]$Port = 8000,
    [switch]$NoWait,
    [switch]$TakeOwnership
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

$listening = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
if ($listening) {
    Write-Host "Ghidra MCP already listening on port $Port (PID $($listening.OwningProcess))."
    exit 0
}

. (Join-Path $PSScriptRoot "tools.ps1")
$ghidra = Find-Ghidra
if (-not $ghidra) {
    throw "Ghidra not found under $DinoTools. Run scripts/setup-tools.ps1 (or set GHIDRA_INSTALL_DIR)."
}
$env:GHIDRA_INSTALL_DIR = $ghidra

# Ghidra 12.2 requires JDK 25.
$jdk = Find-Jdk25
if (-not $jdk) {
    throw "Ghidra 12.2 requires JDK 25. Run scripts/setup-tools.ps1, or set JAVA_HOME to a JDK 25."
}
$env:JAVA_HOME = $jdk
$env:PATH = (Join-Path $jdk "bin") + ";" + $env:PATH
Write-Host "Using JAVA_HOME=$jdk"

$mcp = Join-Path $root ".venv\Scripts\pyghidra-mcp.exe"
if (-not (Test-Path $mcp)) { throw "pyghidra-mcp not installed in .venv" }

$exe = Join-Path $root "game\%MAINDIR%\DinoIsland.exe"
if (-not (Test-Path $exe)) { throw "DinoIsland.exe not found at $exe" }

$project = Join-Path $root "analysis\ghidra"
$logs = Join-Path $root "analysis\logs"
New-Item -ItemType Directory -Force $project, $logs | Out-Null

# A non-shared Ghidra project only opens for the OS user who created it. A
# checkout copied from another contributor keeps their name in project.prp,
# and Ghidra fails with "NotOwnerException: Project is owned by <name>".
$prp = Join-Path $project "dino.rep\project.prp"
if (Test-Path $prp) {
    $owner = ([regex]::Match((Get-Content $prp -Raw), 'NAME="OWNER"[^>]*VALUE="([^"]*)"')).Groups[1].Value
    if ($owner -and $owner -ne $env:USERNAME) {
        if (-not $TakeOwnership) {
            throw ("analysis\ghidra is owned by '$owner', not '$env:USERNAME' (copied from another machine). " +
                "Re-run with -TakeOwnership to keep its analysis (project.prp is backed up first), " +
                "or delete analysis\ghidra to re-import from scratch.")
        }
        Copy-Item $prp "$prp.$owner.bak" -Force
        $text = (Get-Content $prp -Raw) -replace '(NAME="OWNER"[^>]*VALUE=")[^"]*(")', "`${1}$env:USERNAME`${2}"
        # No BOM: Set-Content -Encoding UTF8 in Windows PowerShell 5.1 adds one.
        [System.IO.File]::WriteAllText($prp, $text, (New-Object System.Text.UTF8Encoding($false)))
        Write-Host "Took ownership of analysis\ghidra ($owner -> $env:USERNAME); backup: $prp.$owner.bak"
    }
}

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

# Ghidra keeps its user settings (and the OSGi/Felix script cache) under
# %APPDATA%\ghidra. When this script is launched from a packaged (MSIX) app
# such as the Claude desktop app, Windows silently redirects new files there
# into the app's private LocalCache; Felix then sees a canonical path outside
# its cache dir and every script/bundle fails ("The data file must be inside
# the data dir"). A per-checkout settings dir avoids the redirect and keeps
# contributors' clones independent. Only the server process sees the change.
$settings = Join-Path $root "analysis\ghidra-settings"
New-Item -ItemType Directory -Force $settings | Out-Null
$savedAppData = $env:APPDATA
$env:APPDATA = $settings
try {
    $proc = Start-Process -FilePath $mcp -ArgumentList $args -WorkingDirectory $root `
        -RedirectStandardOutput $outLog -RedirectStandardError $errLog `
        -WindowStyle Hidden -PassThru
} finally {
    $env:APPDATA = $savedAppData
}

Write-Host "Started pyghidra-mcp (PID $($proc.Id))."
Write-Host "  project:  $project"
Write-Host "  settings: $settings"
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
