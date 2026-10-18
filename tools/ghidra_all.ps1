# Run one Ghidra script over every program in the project, one after another (the project allows one writer).
# usage: powershell -File ghidra_all.ps1 <script> [-OutTsvPattern 'D:\trainz12-decomp\out\vtnames_{0}.tsv']
param(
    [Parameter(Mandatory)][string]$Script,
    [string]$OutTsvPattern = ''
)
$run = Join-Path $PSScriptRoot 'ghidra_run.cmd'
# first-run modules were imported as a folder (/bin); WaterSystem, trainznativeinterface and trainz.exe are in the root
$root = @('WaterSystem_7c.dll', 'trainznativeinterface.dll', 'trainz.exe')
$programs = Get-ChildItem 'D:\trainz12-decomp\bin' -File | Select-Object -ExpandProperty Name
foreach ($p in $programs) {
    $folder = if ($root -contains $p) { 'trainz12' } else { 'trainz12/bin' }
    $log = "D:\trainz12-decomp\out\$($Script -replace '\.java$','')_$p.log"
    $scriptArgs = @()
    if ($OutTsvPattern) { $scriptArgs += ($OutTsvPattern -f $p) }
    & $run $log $folder $p $Script @scriptArgs
    $line = Select-String -Path $log -Pattern '> [A-Z]{4,} ' | Select-Object -Last 1
    if ($line) { $line.Line -replace '^INFO\s+\S+> ', '' -replace '\s+\(GhidraScript\)\s*$', '' } else { "$p : no result (see $log)" }
}
