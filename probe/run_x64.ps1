# x64 harness: runs the x64 task/EditItem probes and the golden test, compares each with its 32-bit reference
# (task_orig.txt and edititem_orig.txt from the original module; golden_32.txt from the 32-bit build, which needs
# KernelStandard_7c.dll, so it runs in .\ours). Build first: cmake --build build32 / build64 --config Release.
$root = $PSScriptRoot
New-Item -ItemType Directory -Force "$root\x64" | Out-Null
Copy-Item "$root\orig\plugins.ini" "$root\x64\" -Force
# the plugin system reads plugins.ini next to the exe, so the probes run from copies in .\x64
Copy-Item D:\trainz12-decomp\build64\Release\task_probe_x64.exe, D:\trainz12-decomp\build64\Release\edititem_probe_x64.exe "$root\x64\" -Force
Push-Location "$root\ours"
& D:\trainz12-decomp\build32\Release\golden_kernel.exe ..\golden_32.txt ..\sizes_32.txt
Pop-Location
Push-Location "$root\x64"
Remove-Item -Recurse -Force edtmp, edtmp2 -ErrorAction SilentlyContinue
& .\task_probe_x64.exe ..\task_x64.txt
& .\edititem_probe_x64.exe ..\edititem_x64.txt
& D:\trainz12-decomp\build64\Release\golden_kernel.exe ..\golden_x64.txt ..\sizes_x64.txt
Pop-Location
foreach ($pair in @(@('task_orig', 'task_x64'), @('edititem_orig', 'edititem_x64'), @('golden_32', 'golden_x64'))) {
    $d = @(Compare-Object (Get-Content "$root\$($pair[0]).txt") (Get-Content "$root\$($pair[1]).txt"))
    "$($pair[1]): $((Get-Content "$root\$($pair[1]).txt").Count) lines, $($d.Count) differences"
}
