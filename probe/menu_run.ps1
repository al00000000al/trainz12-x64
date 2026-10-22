# starts the game copy windowed and runs tools/menu_check.ps1 on it (run from the user's terminal)
param([Parameter(Mandatory)][string]$Name)
Start-Process 'D:\games\trainz 2012 x64\bin\trainz.exe' -WorkingDirectory 'D:\games\trainz 2012 x64' -ArgumentList '-windowed','-Width=1280','-Height=800'
& 'D:\universal modder\trainz12-x64\tools\menu_check.ps1' -Name $Name
