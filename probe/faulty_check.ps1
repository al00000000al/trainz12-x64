# starts the game copy windowed, waits, and reports the game's top-level windows with their texts (the "faulty
# built-in assets" error box shows as a window titled "Error"); then closes the game (run from the user's terminal)
param([Parameter(Mandatory)][string]$Name, [int]$WaitSeconds = 40, [switch]$Launcher)
Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class FaultyWin {
    delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr l);
    [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    // presses OK (WM_COMMAND IDOK) on the process's windows titled "Error"
    public static int PressOk(uint pid) {
        int n = 0;
        EnumWindows((h, l) => {
            uint p; GetWindowThreadProcessId(h, out p);
            var sb = new StringBuilder(64); GetWindowText(h, sb, 64);
            if (p == pid && sb.ToString() == "Error") { PostMessage(h, 0x111, (IntPtr)1, IntPtr.Zero); n++; }
            return true;
        }, IntPtr.Zero);
        return n;
    }
    public static List<string> Texts(uint pid) {
        var r = new List<string>();
        EnumWindows((h, l) => {
            uint p; GetWindowThreadProcessId(h, out p);
            if (p != pid || !IsWindowVisible(h)) return true;
            var sb = new StringBuilder(512); GetWindowText(h, sb, 512);
            var line = "window '" + sb + "'";
            EnumChildWindows(h, (c, l2) => { var t = new StringBuilder(512); GetWindowText(c, t, 512); if (t.Length > 0) line += " | " + t; return true; }, IntPtr.Zero);
            r.Add(line);
            return true;
        }, IntPtr.Zero);
        return r;
    }
}
"@
if ($Launcher) {  # the way the user starts it: the root Trainz.exe opens the launcher (and TADDaemon)
    Start-Process 'D:\games\trainz 2012 x64\Trainz.exe' -WorkingDirectory 'D:\games\trainz 2012 x64'
    Start-Sleep ($WaitSeconds + 20)
    $all = Get-Process | Where-Object { $_.Path -like 'D:\games\trainz 2012 x64\*' }
    $texts = foreach ($q in $all) { [FaultyWin]::Texts([uint32]$q.Id) | ForEach-Object { "$($q.ProcessName): $_" } }
    $texts
    $faulty = [bool]($texts | Where-Object { $_ -match 'faulty' })
    "name=$Name launcher faulty_dialog=$faulty"
    foreach ($q in $all) { if ($faulty) { [void][FaultyWin]::PressOk([uint32]$q.Id) } }
    Start-Sleep 3
    foreach ($q in $all) { if (-not $q.HasExited) { [void]$q.CloseMainWindow() } }
    exit 0
}
Start-Process 'D:\games\trainz 2012 x64\bin\trainz.exe' -WorkingDirectory 'D:\games\trainz 2012 x64' -ArgumentList '-windowed','-Width=1280','-Height=800'
$t = 0
while ($t -lt 90 -and -not (Get-Process trainz -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0)) { Start-Sleep 2; $t += 2 }
$p = Get-Process trainz -ErrorAction SilentlyContinue
if (-not $p) { 'game did not start'; exit 1 }
Start-Sleep $WaitSeconds
$texts = [FaultyWin]::Texts([uint32]$p.Id)
$texts
$faulty = [bool]($texts | Where-Object { $_ -match 'faulty' })
"name=$Name faulty_dialog=$faulty"
if ($faulty) { "pressed OK on $([FaultyWin]::PressOk([uint32]$p.Id))"; Start-Sleep 10 }
[void]$p.CloseMainWindow()
if (-not $p.WaitForExit(30000)) { Stop-Process -Id $p.Id -Force; 'killed' }
Copy-Item -LiteralPath 'D:\games\trainz 2012 x64\JetLog.txt' "D:\trainz12-decomp\shots\JetLog_faulty_$Name.txt"
