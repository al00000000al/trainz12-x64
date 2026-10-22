# copies the current task_probe and our Kernel_7c, runs both sides, writes task_orig.txt / task_ours.txt
$root = $PSScriptRoot
Copy-Item D:\trainz12-decomp\build32\Release\Kernel_7c.dll "$root\ours\Kernel_7c.dll" -Force
foreach ($side in 'orig','ours') {
    Copy-Item D:\trainz12-decomp\build32\Release\task_probe.exe "$root\$side\" -Force
    Push-Location "$root\$side"
    $p = Start-Process -FilePath ".\task_probe.exe" -ArgumentList "..\task_$side.txt" -NoNewWindow -Wait -PassThru
    "$side exit=$($p.ExitCode)"
    Pop-Location
}
