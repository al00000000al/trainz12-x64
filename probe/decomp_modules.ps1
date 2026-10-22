# decompiles the given modules from the main Ghidra project into D:\trainz12-decomp\decomp
$run = 'D:\universal modder\trainz12-x64\tools\ghidra_run.cmd'
foreach ($m in $args) {
    & $run "D:\trainz12-decomp\out\DecompileAll_$m.log" 'trainz12/bin' "$m.dll" DecompileAll.java
    $f = "D:\trainz12-decomp\decomp\$m.dll.c"
    if (Test-Path $f) { "$m : $((Get-Item $f).Length) bytes" } else { "$m : no output (see log)" }
}
