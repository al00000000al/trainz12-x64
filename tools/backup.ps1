# Snapshot the private decompilation workspace and this project into a dated 7z archive with a SHA-256 file.
# Copy the .7z and .sha256 to any other media (external disk, USB, cloud). Never publish the archive.
# usage: powershell -File backup.ps1 [-Dest D:\trainz12-backups]
param([string]$Dest = 'D:\trainz12-backups')

$ErrorActionPreference = 'Stop'
$sevenZip = 'C:\Program Files\7-Zip\7z.exe'
$stamp = Get-Date -Format 'yyyy-MM-dd_HHmm'
New-Item -ItemType Directory -Force $Dest | Out-Null
$archive = Join-Path $Dest "trainz12-decomp_$stamp.7z"

# Ghidra must not be writing the project while it is archived
if (Get-Process java -ErrorAction SilentlyContinue | Where-Object { $_.Path -like '*Adoptium*' }) {
    throw 'Ghidra (java) is running; wait for it to finish before backing up.'
}

# the unpacked Jet SDK is left out: it is rebuilt from D:\download\auran-jet-demo with unshield (see MODLOG)
& $sevenZip a -t7z -mx=7 -bso0 -bsp0 '-xr!jet-sdk' $archive 'D:\trainz12-decomp' (Split-Path $PSScriptRoot -Parent)
if ($LASTEXITCODE -ne 0) { throw "7z failed with $LASTEXITCODE" }
& $sevenZip t -bso0 -bsp0 $archive
if ($LASTEXITCODE -ne 0) { throw "archive test failed with $LASTEXITCODE" }

$hash = (Get-FileHash $archive -Algorithm SHA256).Hash
"$hash  $(Split-Path $archive -Leaf)" | Set-Content -Encoding ascii "$archive.sha256"
'{0}  {1:N0} MB  sha256 {2}' -f $archive, ((Get-Item $archive).Length / 1MB), $hash
