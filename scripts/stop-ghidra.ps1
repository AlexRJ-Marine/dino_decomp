param([int]$Port = 8000)

$connections = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
if (-not $connections) {
    Write-Host "Nothing is listening on port $Port."
    exit 0
}

foreach ($pid in ($connections.OwningProcess | Select-Object -Unique)) {
    $p = Get-Process -Id $pid -ErrorAction SilentlyContinue
    if ($p) {
        Write-Host "Stopping $($p.ProcessName) (PID $pid)..."
        Stop-Process -Id $pid -Force
    }
}
Write-Host "Stopped.";
