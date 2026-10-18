# After the game copy was started (from the user's terminal, outside the app sandbox): wait for the main menu,
# let it settle, close it gracefully, save JetLog as <Name> and print the string-pool line and the diff against
# the baseline log.
# usage: powershell -File menu_check.ps1 -Name fda_half1
param([Parameter(Mandatory)][string]$Name, [int]$SettleSeconds = 45)
$t = 0
while ($t -lt 90 -and -not (Get-Process trainz -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0)) { Start-Sleep 2; $t += 2 }
$p = Get-Process trainz -ErrorAction SilentlyContinue
if (-not $p) { 'game did not start'; exit 1 }
Start-Sleep $SettleSeconds
$alive = -not $p.HasExited
[void]$p.CloseMainWindow()
$exited = $p.WaitForExit(30000)
$log = "D:\trainz12-decomp\shots\JetLog_$Name.txt"
Copy-Item -LiteralPath 'D:\games\trainz 2012 x64\JetLog.txt' $log
# the "** Packed Heap Summary **" pool lines at exit vary between identical runs (e.g. #StringData 507k/508k
# with the very same DLL), so they are left out of the comparison
$strip = { param($f) Get-Content $f | ForEach-Object { $_ -replace '^\?\s+[\d:.]+ : ', '' } | Where-Object { $_ -notmatch ' - \d+k unused, \d+k used, \d+k free$' } }
$diff = @(Compare-Object (& $strip 'D:\trainz12-decomp\shots\JetLog_baseline.txt') (& $strip $log))
"alive_at_menu=$alive exited=$exited diff_lines=$($diff.Count)"
(Select-String -LiteralPath $log -Pattern 'StringData').Line -replace '^\?\s+[\d:.]+ : ', ''
