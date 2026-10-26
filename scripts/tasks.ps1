param(
    [switch]$Update,
    [switch]$Detail,
    [string]$Out = "notes\OPEN-TASKS.md"
)

$ErrorActionPreference = "Stop"
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch { }
$root = Split-Path $PSScriptRoot -Parent
$tasksPath = Join-Path $root "TASKS.md"
if (-not (Test-Path $tasksPath)) { throw "missing TASKS.md" }

$utf8 = New-Object System.Text.UTF8Encoding($false)
$lines = [System.IO.File]::ReadAllLines($tasksPath, [System.Text.Encoding]::UTF8)

$heading = "(top)"
$items = [System.Collections.Generic.List[object]]::new()
for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]
    if ($line -match '^#{2,4}\s+(.*)$') {
        $heading = $Matches[1].Trim()
        continue
    }
    if ($line -match '^\s*- \[ \]\s+(.*)$') {
        $text = $Matches[1].Trim()
        $detailLines = [System.Collections.Generic.List[string]]::new()
        $j = $i + 1
        while ($j -lt $lines.Count -and
               $lines[$j].Trim() -ne "" -and
               $lines[$j] -notmatch '^\s*- \[' -and
               $lines[$j] -notmatch '^#{2,4}\s') {
            $detailLines.Add($lines[$j].TrimEnd())
            $j++
        }
        $items.Add([pscustomobject]@{
            Heading = $heading
            Text = $text
            Detail = $detailLines
        })
        $i = $j - 1
    }
}

$result = [System.Collections.Generic.List[string]]::new()
$result.Add("# Open tasks")
$result.Add("")
$result.Add("Generated from ``TASKS.md`` on $((Get-Date).ToString('yyyy-MM-dd HH:mm')). Do not edit; regenerate with ``scripts/tasks.ps1 -Update``.")
$result.Add("")
$result.Add("$($items.Count) open items.")
$current = ""
foreach ($item in $items) {
    if ($item.Heading -ne $current) {
        $result.Add("")
        $result.Add("## $($item.Heading)")
        $current = $item.Heading
    }
    $result.Add("- [ ] $($item.Text)")
    if ($Detail) {
        foreach ($d in $item.Detail) { $result.Add("  " + $d.Trim()) }
    }
}
$text = ($result -join "`r`n") + "`r`n"

if ($Update) {
    $path = Join-Path $root $Out
    [System.IO.File]::WriteAllText($path, $text, $utf8)
    Write-Host "wrote $Out ($($items.Count) open items)"
} else {
    Write-Output $text
}
