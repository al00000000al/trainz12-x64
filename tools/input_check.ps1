# After the game copy was started (from the user's terminal): wait for the main menu, then drive it with real
# input (cursor moves, clicks, keys through SendInput) and save a screenshot of the game window after each step.
# Run once with the original module and once with ours, then compare the shots.
# The steps are a list of "move x y", "click x y", "rclick x y", "key VK", "wait ms", "dshot name" (the window's
# client area through desktop duplication, tools/dupshot.cpp), "shot name" (a GDI copy: black for the GL view) and
# "screen name" (the whole screen, GDI), where x, y are client coordinates; ';' separates steps.
# Not Print Screen: on Windows 11 it opens the Snipping Tool overlay, which takes the focus.
# usage: powershell -File input_check.ps1 -Name ours -Steps "dshot menu; move 200 300; dshot hover"
param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Steps, [int]$SettleSeconds = 45,
      [string]$ShotDir = 'D:\trainz12-decomp\shots')
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class In {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int l, t, r, b; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int x, y; }
    [StructLayout(LayoutKind.Sequential)] struct KEYBDINPUT { public ushort vk, scan; public uint flags, time; public IntPtr extra; }
    [StructLayout(LayoutKind.Sequential)] struct MOUSEINPUT { public int dx, dy; public uint data, flags, time; public IntPtr extra; }
    [StructLayout(LayoutKind.Explicit)] struct INPUT {
        [FieldOffset(0)] public uint type;
        [FieldOffset(8)] public KEYBDINPUT ki;   // 64-bit PowerShell: the union starts at 8
        [FieldOffset(8)] public MOUSEINPUT mi;
    }
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    public static void TopMost(IntPtr h) { SetWindowPos(h, new IntPtr(-1), 0, 0, 0, 0, 3); }  // NOSIZE | NOMOVE
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] static extern uint SendInput(uint n, INPUT[] i, int size);
    public static void Mouse(uint flags) {
        var i = new INPUT[1]; i[0].type = 0; i[0].mi.flags = flags;
        SendInput(1, i, Marshal.SizeOf(typeof(INPUT)));
    }
    public static void Key(ushort vk) {
        var i = new INPUT[2];
        i[0].type = 1; i[0].ki.vk = vk;
        i[1].type = 1; i[1].ki.vk = vk; i[1].ki.flags = 2;
        SendInput(2, i, Marshal.SizeOf(typeof(INPUT)));
    }
}
'@
[void][In]::SetProcessDPIAware()  # window rectangles and cursor positions in physical pixels
$t = 0
while ($t -lt 90 -and -not (Get-Process trainz -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0)) { Start-Sleep 2; $t += 2 }
$p = Get-Process trainz -ErrorAction SilentlyContinue
if (-not $p) { 'game did not start'; exit 1 }
Start-Sleep $SettleSeconds
$h = $p.MainWindowHandle
function Origin { $o = New-Object In+POINT; [void][In]::ClientToScreen($h, [ref]$o); $o }
function Shot([string]$n) {
    $r = New-Object In+RECT; [void][In]::GetClientRect($h, [ref]$r); $o = Origin
    $bmp = New-Object System.Drawing.Bitmap ($r.r - $r.l), ($r.b - $r.t)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $dc = $g.GetHdc(); [void][In]::PrintWindow($h, $dc, 3); $g.ReleaseHdc($dc)  # client only, full content (GL / D3D)
    $file = Join-Path $ShotDir "input_${Name}_$n.png"
    $bmp.Save($file, [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
    "shot $file"
}
[In]::TopMost($h)  # other windows popping up would take the clicks and cover the shots
[void][In]::SetForegroundWindow($h)
Start-Sleep -Milliseconds 500
foreach ($step in ($Steps -split ';')) {
    $w = -split $step.Trim()
    if (-not $w) { continue }
    [void][In]::SetForegroundWindow($h)
    switch ($w[0]) {
        'move'   { $o = Origin; [void][In]::SetCursorPos($o.x + [int]$w[1], $o.y + [int]$w[2]); Start-Sleep -Milliseconds 300 }
        'click'  { $o = Origin; [void][In]::SetCursorPos($o.x + [int]$w[1], $o.y + [int]$w[2]); Start-Sleep -Milliseconds 200
                   [In]::Mouse(2); Start-Sleep -Milliseconds 80; [In]::Mouse(4); Start-Sleep -Milliseconds 500 }
        'rclick' { $o = Origin; [void][In]::SetCursorPos($o.x + [int]$w[1], $o.y + [int]$w[2]); Start-Sleep -Milliseconds 200
                   [In]::Mouse(8); Start-Sleep -Milliseconds 80; [In]::Mouse(16); Start-Sleep -Milliseconds 500 }
        'key'    { [In]::Key([uint16]$w[1]); Start-Sleep -Milliseconds 400 }
        'wait'   { Start-Sleep -Milliseconds ([int]$w[1]) }
        'shot'   { Shot $w[1] }
        'dshot'  { # desktop duplication (tools/dupshot.cpp, built to D:\trainz12-decomp\bin): sees the GL view
                   $r = New-Object In+RECT; [void][In]::GetClientRect($h, [ref]$r); $o = Origin
                   $file = Join-Path $ShotDir "input_${Name}_$($w[1]).png"
                   & 'D:\trainz12-decomp\bin\dupshot.exe' $file $o.x $o.y ($r.r - $r.l) ($r.b - $r.t) | Out-Null
                   "dshot $file" }
        'screen' { $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
                   $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
                   $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.X, $b.Y, 0, 0, $bmp.Size)
                   $file = Join-Path $ShotDir "input_${Name}_$($w[1]).png"
                   $bmp.Save($file, [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose(); "screen $file" }
    }
}
$alive = -not $p.HasExited
if ($alive) { [void]$p.CloseMainWindow(); $exited = $p.WaitForExit(30000) } else { $exited = $true }
"alive_after_steps=$alive exited=$exited"
