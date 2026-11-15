param(
    [switch]$PrepareOnly,
    [switch]$BackportOnly,
    [switch]$NoAudio,
    [ValidateRange(0,3600)][int]$Seconds=0,
    [ValidateSet('Debug','Release')][string]$Configuration='Debug'
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
& python -B (Join-Path $PSScriptRoot 'tools/backport_xxx_xs.py') --check
if($LASTEXITCODE -ne 0) { throw 'Backport checks failed. Regenerate with python -B rebuild/tools/backport_xxx_xs.py.' }
$taskOverlay=Join-Path $PSScriptRoot ('runtime/xxx-xs-data-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskOverlay | Out-Null
$taskAssets=Join-Path $taskRoot 'assets'
foreach($taskPackage in Get-ChildItem -LiteralPath $taskAssets -Directory) {
    if($BackportOnly -and $taskPackage.Name -ne 'Sven2') { continue }
    Copy-Item -LiteralPath $taskPackage.FullName -Destination $taskOverlay -Recurse
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'backports/XXX_XS') -Destination $taskOverlay -Recurse
$taskPack=Join-Path $taskOverlay 'XXX_XS'
$taskManifest=Get-Content -LiteralPath (Join-Path $taskPack 'manifest.json') -Raw | ConvertFrom-Json
foreach($taskEntry in $taskManifest.files_sha256.PSObject.Properties) {
    if((Get-FileHash -LiteralPath (Join-Path $taskPack $taskEntry.Name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskEntry.Value) { throw "Backport copy mismatch: $($taskEntry.Name)" }
}
Write-Host "XXX XS data: $taskOverlay"
Write-Host 'Start -> XXX_XS/Training, XXX_XS/Garten 01 or XXX_XS/Garten 02. Default catalog also includes all original Zwo packs.'
if($PrepareOnly) { return }
$taskExecutable=Join-Path $PSScriptRoot "bin/Win32/$Configuration/Sven2Bootstrap.exe"
if(!(Test-Path -LiteralPath $taskExecutable)) { throw 'Build the game first with rebuild/Build.ps1.' }
$taskArguments=@('--game-dir',(Join-Path $taskRoot 'games/sven2'),'--data-dir',$taskOverlay)
if($NoAudio) { $taskArguments+='--no-audio' }
if($Seconds) { $taskArguments+=@('--seconds',"$Seconds") }
& $taskExecutable @taskArguments
exit $LASTEXITCODE
