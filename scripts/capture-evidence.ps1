param(
    [Parameter(Mandatory = $true)][string]$Name,
    [string]$Command = "",
    [string]$Path = "",
    [string]$Note = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$dir = Join-Path $root "notes\harness"
if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force $dir | Out-Null }
$manifest = Join-Path $dir "manifest.md"
$utf8 = New-Object System.Text.UTF8Encoding($false)

$hash = (& git -C $root rev-parse --short HEAD 2>$null)
if (-not $hash) { $hash = "unknown" }
$stamp = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
$rel = $Path
if ($Path -and (Test-Path $Path)) {
    $rel = (Resolve-Path $Path).Path
    if ($rel.StartsWith($root)) {
        $rel = $rel.Substring($root.Length).TrimStart('\')
    }
}

if (-not (Test-Path $manifest)) {
    $header = @(
        "# Harness manifest",
        "",
        "Local, gitignored. One row per captured run.",
        "",
        "| Time | Commit | Name | Command | Artifact | Note |",
        "| --- | --- | --- | --- | --- | --- |"
    ) -join "`r`n"
    [System.IO.File]::WriteAllText($manifest, $header + "`r`n", $utf8)
}

$row = "| $stamp | $hash | $Name | $Command | $rel | $Note |"
[System.IO.File]::AppendAllText($manifest, $row + "`r`n", $utf8)
Write-Host "recorded: $row"
Write-Host "manifest: $manifest"
