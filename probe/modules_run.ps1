# starts the game copy, waits for the menu, lists the Jet modules loaded in it (32-bit PowerShell sees WOW64
# modules), then closes the game. usage: powershell -File modules_run.ps1
Start-Process 'D:\games\trainz 2012 x64\bin\trainz.exe' -WorkingDirectory 'D:\games\trainz 2012 x64' -ArgumentList '-windowed','-Width=1280','-Height=800'
$t = 0
while ($t -lt 90 -and -not (Get-Process trainz -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0)) { Start-Sleep 2; $t += 2 }
Start-Sleep 30
$ps32 = "$env:WINDIR\SysWOW64\WindowsPowerShell\v1.0\powershell.exe"
& $ps32 -NoProfile -Command "(Get-Process trainz).Modules | Where-Object { `$_.ModuleName -match '_7c|Input' } | ForEach-Object { `$_.ModuleName } | Sort-Object" | Out-File -Encoding utf8 'D:\trainz12-decomp\probe\modules_loaded.txt'
$p = Get-Process trainz -ErrorAction SilentlyContinue
if ($p) { [void]$p.CloseMainWindow(); [void]$p.WaitForExit(30000) }
Get-Content 'D:\trainz12-decomp\probe\modules_loaded.txt'
