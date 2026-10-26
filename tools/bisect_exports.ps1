# Build a Kernel_7c.dll that implements the base set plus only the given slice of a candidate export group,
# deploy it to the game copy. Used to find which export changes in-game behaviour.
# usage: powershell -File bisect_exports.ps1 -Pattern 'FastDynamicArray@PAVPluginConfig' -From 0 -Count 16
param(
    [Parameter(Mandatory)][string]$Pattern,
    [int]$From = 0,
    [int]$Count = 1000
)
$ErrorActionPreference = 'Stop'
$impl = 'D:\trainz12-decomp\src\jet\core\Kernel_7c\implemented.txt'
$backup = "$impl.full"
if (-not (Test-Path $backup)) { Copy-Item $impl $backup }
$all = Get-Content $backup
$group = @($all | Where-Object { $_ -match [regex]::Escape($Pattern) -and $_ -notmatch '^#' })
$keep = @($group | Select-Object -Skip $From -First $Count)
$out = $all | Where-Object { -not ($group -contains $_) -or ($keep -contains $_) }
Set-Content -Path $impl -Value $out -Encoding ascii
$env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
cmake --build 'D:\trainz12-decomp\build32' --config Release --target Kernel_7c 2>&1 | Select-String -Pattern ' error |implemented' | ForEach-Object { $_.Line }
Copy-Item 'D:\trainz12-decomp\build32\Release\Kernel_7c.dll' 'D:\games\trainz 2012 x64\bin\Kernel_7c.dll' -Force
Remove-Item -LiteralPath 'D:\games\trainz 2012 x64\JetLog.txt' -ErrorAction SilentlyContinue
"group $($group.Count), kept $From..$($From + $keep.Count - 1): " + (($keep | ForEach-Object { ($_ -split '@')[0] }) -join ' ')
# restore the full list (the deployed DLL keeps the slice)
Copy-Item $backup $impl -Force
