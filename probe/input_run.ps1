# starts the game copy windowed and drives it with tools/input_check.ps1 (run from the user's terminal)
param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Steps)
Remove-Item 'D:\trainz12-decomp\probe\iw32_trace.txt' -ErrorAction SilentlyContinue
Start-Process 'D:\games\trainz 2012 x64\bin\trainz.exe' -WorkingDirectory 'D:\games\trainz 2012 x64' -ArgumentList '-windowed','-Width=1280','-Height=800'
& 'D:\universal modder\trainz12-x64\tools\input_check.ps1' -Name $Name -Steps $Steps
if (Test-Path 'D:\trainz12-decomp\probe\iw32_trace.txt') { "trace lines: " + (Get-Content 'D:\trainz12-decomp\probe\iw32_trace.txt').Count }
