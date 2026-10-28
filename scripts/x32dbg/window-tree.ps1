# List every top-level and child window owned by a process, with class,
# title, style, owner, and screen rect. Read-only diagnostic for the harness.
param(
    [string]$ProcessName = 'DinoIsland',
    [int]$ProcessId = 0
)

$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class DinoWindowTree {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, EnumProc p, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h, uint cmd);
  [DllImport("user32.dll")] public static extern int GetWindowLongA(IntPtr h, int idx);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
  public static List<IntPtr> TopLevel(uint pid) {
    var list = new List<IntPtr>();
    EnumWindows((h, l) => {
      uint p; GetWindowThreadProcessId(h, out p);
      if (p == pid) list.Add(h);
      return true;
    }, IntPtr.Zero);
    return list;
  }
  public static List<IntPtr> Children(IntPtr parent) {
    var list = new List<IntPtr>();
    EnumChildWindows(parent, (h, l) => { list.Add(h); return true; }, IntPtr.Zero);
    return list;
  }
}
"@

if ($ProcessId -eq 0) {
    $proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $proc) { throw "process not found: $ProcessName" }
    $ProcessId = $proc.Id
}

function Show-Window([IntPtr]$h, [string]$indent) {
    $cls = New-Object System.Text.StringBuilder 256
    [DinoWindowTree]::GetClassName($h, $cls, $cls.Capacity) | Out-Null
    $txt = New-Object System.Text.StringBuilder 256
    [DinoWindowTree]::GetWindowText($h, $txt, $txt.Capacity) | Out-Null
    $r = New-Object DinoWindowTree+RECT
    [DinoWindowTree]::GetWindowRect($h, [ref]$r) | Out-Null
    $style = [DinoWindowTree]::GetWindowLongA($h, -16)
    $exstyle = [DinoWindowTree]::GetWindowLongA($h, -20)
    $thread = 0
    [DinoWindowTree]::GetWindowThreadProcessId($h, [ref]$thread) | Out-Null
    '{0}hwnd=0x{1:X8} class={2} title="{3}" rect={4},{5} {6}x{7} style=0x{8:X8} ex=0x{9:X8} visible={10} enabled={11}' -f `
        $indent, $h.ToInt64(), $cls.ToString(), $txt.ToString(),
        $r.Left, $r.Top, ($r.Right - $r.Left), ($r.Bottom - $r.Top),
        $style, $exstyle, [DinoWindowTree]::IsWindowVisible($h), [DinoWindowTree]::IsWindowEnabled($h)
}

Write-Host "process pid=$ProcessId"
$top = [DinoWindowTree]::TopLevel([uint32]$ProcessId)
foreach ($h in $top) {
    Show-Window $h ''
    foreach ($c in [DinoWindowTree]::Children($h)) {
        Show-Window $c '  '
    }
}
