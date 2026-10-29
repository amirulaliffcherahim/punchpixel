# Drives PunchPixelSimulator.exe: launches it, posts key messages straight to
# its window (no focus needed), screenshots each step and checks the screen
# changed and that the service played sound and vibration. Exit 0 only if all hold.
# Usage: powershell -File drive.ps1 [-OutDir <dir>]
param([string]$OutDir = "$PSScriptRoot\shots")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out R r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref P p);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint c, uint t);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  public struct R { public int L, T, Rt, B; }
  public struct P { public int X, Y; }
}
"@

$exe = Resolve-Path "$PSScriptRoot\..\..\Software\Apps\LVGL-GUI\simulator\build\bin\PunchPixelSimulator.exe"
New-Item -ItemType Directory -Force $OutDir | Out-Null
Remove-Item "$OutDir\*.png" -ErrorAction SilentlyContinue
$p = Start-Process $exe -WorkingDirectory (Split-Path $exe) -PassThru -RedirectStandardOutput "$OutDir\sim.log" -RedirectStandardError "$OutDir\sim.err"
$script:fail = @()

function Check([bool]$ok, [string]$what) {
    if ($ok) { "  ok   $what" } else { "  FAIL $what"; $script:fail += $what }
}
function Grab {
    [void][W]::SetWindowPos($h, [IntPtr](-1), 0, 0, 0, 0, 0x0003)
    Start-Sleep -Milliseconds 40
    $r = New-Object W+R; [void][W]::GetClientRect($h, [ref]$r)
    $pt = New-Object W+P; [void][W]::ClientToScreen($h, [ref]$pt)
    $bmp = New-Object Drawing.Bitmap $r.Rt, $r.B
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($pt.X, $pt.Y, 0, 0, (New-Object Drawing.Size $r.Rt, $r.B))
    $g.Dispose(); $bmp
}
function Shot([string]$name) { $b = Grab; $b.Save("$OutDir\$name.png", [Drawing.Imaging.ImageFormat]::Png); $b.Dispose() }
function Sig {
    $b = Grab; $sum = 0L
    for ($y = 0; $y -lt $b.Height; $y += 6) { for ($x = 0; $x -lt $b.Width; $x += 6) {
        $c = $b.GetPixel($x, $y); $sum = ($sum * 31 + $c.R * 3 + $c.G * 5 + $c.B * 7) % 1000000007 } }
    $b.Dispose(); $sum
}
function Tap([char]$k, [int]$holdMs = 50) {
    $vk = [uint32][char]::ToUpper($k)
    $sc = [W]::MapVirtualKey($vk, 0)
    [void][W]::PostMessage($h, 0x0100, [IntPtr][int]$vk, [IntPtr](1 -bor ($sc -shl 16)))
    Start-Sleep -Milliseconds $holdMs
    [void][W]::PostMessage($h, 0x0101, [IntPtr][int]$vk, [IntPtr]([int64](1 -bor ($sc -shl 16) -bor 0xC0000000)))
}

try {
    $h = [IntPtr]::Zero
    for ($i = 0; $i -lt 50 -and $h -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 200; $p.Refresh(); $h = $p.MainWindowHandle }
    if ($h -eq [IntPtr]::Zero) { throw "no window" }
    Start-Sleep -Milliseconds 1500

    "== title"
    Shot "01_title"; $s0 = Sig
    Start-Sleep -Milliseconds 500
    Check ((Sig) -ne $s0) "title animates on its own"

    "== start (R1)"
    Tap '3'; Start-Sleep -Milliseconds 600
    Shot "02_round"; $s1 = Sig
    Check ($s1 -ne $s0) "R1 starts round 1"
    Start-Sleep -Milliseconds 1500
    Shot "03_fight"

    "== jab, jab, weave"
    Tap '1'; Start-Sleep -Milliseconds 160; Shot "04_jab_left"
    Start-Sleep -Milliseconds 250
    Tap '3'; Start-Sleep -Milliseconds 160; Shot "05_jab_right"
    Start-Sleep -Milliseconds 250
    Tap '2'; Start-Sleep -Milliseconds 120; Shot "06_weave_left"

    "== dempsey roll: L2 R2 L2 R2 then punch"
    Start-Sleep -Milliseconds 400
    foreach ($k in '2','4','2','4') { Tap $k 40; Start-Sleep -Milliseconds 200 }
    Start-Sleep -Milliseconds 100
    Shot "07_roll_ready"
    Tap '1'
    Start-Sleep -Milliseconds 500
    Shot "08_dempsey"
    Start-Sleep -Milliseconds 900
    Shot "09_after_roll"

    "== hold R2 pauses"
    Tap '4' 1100
    Start-Sleep -Milliseconds 300
    Shot "10_pause"; $sp = Sig
    Start-Sleep -Milliseconds 600
    Check ((Sig) -eq $sp) "pause screen holds still"
    Tap '1'; Start-Sleep -Milliseconds 300
    Check ((Sig) -ne $sp) "top button resumes"

    "== hold R2 twice quits"
    Tap '4' 1100; Start-Sleep -Milliseconds 300
    Tap '4' 1100; Start-Sleep -Milliseconds 300
    $exited = $p.WaitForExit(4000)
    Check $exited "hold R2 on the pause menu quits the app"
} finally {
    if (-not $p.HasExited) { $p.Kill() }
}

"== service played the cues (log flushed on exit)"
Start-Sleep -Milliseconds 300
$log = [string](Get-Content "$OutDir\sim.log" -Raw)
Check ([bool]($log -match "Mock\.Vibro::play")) "vibro played"
Check ([bool]($log -match "Mock\.Buzzer::play")) "buzzer played"
Check ([bool]($log -match "effect=52,")) "Dempsey Roll cue (PULSING_STRONG 52) played"
Check ([bool]($log -match "2200 Hz")) "Dempsey fanfare reached its top note"

if ($script:fail.Count) { "FAILED: $($script:fail.Count)"; exit 1 } else { "ALL OK"; exit 0 }
