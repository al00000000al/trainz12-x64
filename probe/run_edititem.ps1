# copies the current edititem_probe and our Kernel_7c, runs both sides, writes edititem_orig.txt / edititem_ours.txt
$root = $PSScriptRoot
Copy-Item D:\trainz12-decomp\build32\Release\Kernel_7c.dll "$root\ours\Kernel_7c.dll" -Force
foreach ($side in 'orig','ours') {
    Copy-Item D:\trainz12-decomp\build32\Release\edititem_probe.exe "$root\$side\" -Force
    Push-Location "$root\$side"
    Remove-Item -Recurse -Force edtmp, edtmp2 -ErrorAction SilentlyContinue
    $p = Start-Process -FilePath ".\edititem_probe.exe" -ArgumentList "..\edititem_$side.txt" -NoNewWindow -Wait -PassThru
    "$side exit=$($p.ExitCode)"
    Pop-Location
}
