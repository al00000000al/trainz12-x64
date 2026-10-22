# rebuilds nothing: copies the current probe and our Kernel_7c, runs both sides, writes orig.txt / ours.txt
$root = $PSScriptRoot
Copy-Item D:\trainz12-decomp\build32\Release\Kernel_7c.dll "$root\ours\Kernel_7c.dll" -Force
foreach ($side in 'orig','ours') {
    Copy-Item D:\trainz12-decomp\build32\Release\plugin_probe.exe "$root\$side\" -Force
    Push-Location "$root\$side"
    $p = Start-Process -FilePath ".\plugin_probe.exe" -ArgumentList 'plugins.ini',"..\$side.txt" -NoNewWindow -Wait -PassThru
    "$side exit=$($p.ExitCode)"
    Pop-Location
}
