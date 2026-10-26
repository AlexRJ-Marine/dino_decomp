param(
    [string]$Done = "",
    [string]$Next = "",
    [string]$Focus = "",
    [string]$Blocked = "",
    [switch]$Show,
    [int]$KeepRecent = 8,
    [int]$KeepNext = 10
)

$ErrorActionPreference = "Stop"
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch { }
$root = Split-Path $PSScriptRoot -Parent
$nowPath = Join-Path $root "notes\NOW.md"
$historyDir = Join-Path $root "notes\history"
$utf8 = New-Object System.Text.UTF8Encoding($false)

if (-not (Test-Path $nowPath)) { throw "missing notes/NOW.md" }
$lines = [System.Collections.Generic.List[string]]::new(
    [System.IO.File]::ReadAllLines($nowPath, [System.Text.Encoding]::UTF8))
$today = Get-Date -Format "yyyy-MM-dd"

function Replace-Timestamp {
    for ($i = 0; $i -lt $script:lines.Count; $i++) {
        if ($script:lines[$i] -match '^Updated:') {
            $script:lines[$i] = "Updated: $script:today"
            return
        }
    }
}

function Add-Bullet([string]$heading, [string]$text, [int]$keep) {
    $start = -1
    for ($i = 0; $i -lt $script:lines.Count; $i++) {
        if ($script:lines[$i].Trim() -eq $heading) { $start = $i; break }
    }
    if ($start -lt 0) { throw "section '$heading' not found" }
    $insert = $start + 1
    while ($insert -lt $script:lines.Count -and
           $script:lines[$insert].Trim() -eq "") { $insert++ }
    if ($insert -lt $script:lines.Count -and
        $script:lines[$insert].Trim() -eq "- None.") {
        $script:lines.RemoveAt($insert)
    }
    $script:lines.Insert($insert, "- $text")
    $seen = 0
    $i = $start + 1
    while ($i -lt $script:lines.Count) {
        if ($script:lines[$i] -match '^## ') { break }
        if ($script:lines[$i] -match '^- ') {
            $seen++
            if ($seen -gt $keep) { $script:lines.RemoveAt($i); continue }
        }
        $i++
    }
}

function Set-Section([string]$heading, [string]$text) {
    $start = -1
    for ($i = 0; $i -lt $script:lines.Count; $i++) {
        if ($script:lines[$i].Trim() -eq $heading) { $start = $i; break }
    }
    if ($start -lt 0) { throw "section '$heading' not found" }
    $end = $start + 1
    while ($end -lt $script:lines.Count -and
           $script:lines[$end] -notmatch '^## ') { $end++ }
    $script:lines.RemoveRange($start + 1, $end - $start - 1)
    $script:lines.Insert($start + 1, "")
    $script:lines.Insert($start + 2, "- $text")
}

if ($Done) {
    Add-Bullet "## Recent" "$today - $Done" $KeepRecent
    if (-not (Test-Path $historyDir)) {
        New-Item -ItemType Directory $historyDir | Out-Null
    }
    $historyPath = Join-Path $historyDir ("{0:yyyy-MM}.md" -f (Get-Date))
    if (-not (Test-Path $historyPath)) {
        [System.IO.File]::WriteAllText(
            $historyPath,
            "# Completion log - $((Get-Date).ToString('yyyy-MM'))`r`n`r`n",
            $utf8)
    }
    [System.IO.File]::AppendAllText(
        $historyPath, "- $today - $Done`r`n", $utf8)
}
if ($Next) { Add-Bullet "## Next actions" $Next $KeepNext }
if ($Focus) { Add-Bullet "## Focus" $Focus 8 }
if ($Blocked) {
    if ($Blocked -eq "none") { Set-Section "## Blockers" "None." }
    else { Set-Section "## Blockers" $Blocked }
}

Replace-Timestamp
[System.IO.File]::WriteAllLines($nowPath, $lines, $utf8)

if ($Show -or (-not $Done -and -not $Next -and -not $Focus -and -not $Blocked)) {
    Get-Content $nowPath | Select-Object -First 80
} else {
    Write-Host "updated notes/NOW.md"
}
