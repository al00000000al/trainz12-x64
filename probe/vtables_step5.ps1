# dumps the vftables of the step-5 modules into D:\trainz12-decomp\out\vtables_<module>.txt
$run = 'D:\universal modder\trainz12-x64\tools\ghidra_run.cmd'
foreach ($m in $args) {
    & $run "D:\trainz12-decomp\out\DumpVtables_$m.log" 'trainz12/bin' "$m.dll" DumpVtables.java '.' "D:\trainz12-decomp\out\vtables_$m.txt"
    $f = "D:\trainz12-decomp\out\vtables_$m.txt"
    if (Test-Path $f) { "$m : $((Select-String -Path $f -Pattern '^==').Count) tables" } else { "$m : no output (see log)" }
}
