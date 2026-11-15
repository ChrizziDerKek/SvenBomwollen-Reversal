param(
    [switch]$PrepareOnly,
    [switch]$NoAudio,
    [ValidateRange(0,3600)][int]$Seconds=0,
    [ValidateSet('Debug','Release')][string]$Configuration='Debug'
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskPack=Join-Path $PSScriptRoot 'examplepacks/WeekendAdventure'
& python -B (Join-Path $PSScriptRoot 'tools/create_example_pack.py') --check
if($LASTEXITCODE -ne 0) { throw 'Example pack structural/determinism checks failed.' }
# A fresh directory on every preparation prevents overwriting hard links,
# stale descriptors, or another running session. All resources are real copies.
$taskOverlay=Join-Path $PSScriptRoot ('runtime/weekend-data-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskOverlay | Out-Null
foreach($taskPackage in @('Sven2','XS','01','02','XXL')) {
    $taskSource=Join-Path $taskRoot "assets/$taskPackage"
    foreach($taskFile in Get-ChildItem -LiteralPath $taskSource -File -Recurse) {
        $taskRelative=$taskFile.FullName.Substring($taskSource.Length).TrimStart('\','/')
        # Theme packages provide artwork/audio; Sven2.xml provides engine resources.
        # Original maps and unrelated catalog entries are deliberately omitted.
        if($taskRelative -match '(^|[\\/])levels[\\/]' -or ($taskPackage -ne 'Sven2' -and $taskFile.Extension -eq '.xml')) { continue }
        $taskDestination=Join-Path (Join-Path $taskOverlay $taskPackage) $taskRelative
        New-Item -ItemType Directory -Path (Split-Path $taskDestination -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $taskFile.FullName -Destination $taskDestination
    }
}
$taskPackTarget=Join-Path $taskOverlay 'WeekendAdventure'
New-Item -ItemType Directory -Path (Join-Path $taskPackTarget 'levels') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $taskPack 'WeekendAdventure.xml') -Destination $taskPackTarget
Copy-Item -LiteralPath (Join-Path $taskPack 'manifest.json') -Destination $taskPackTarget
Get-ChildItem -LiteralPath (Join-Path $taskPack 'levels') -File -Filter '*.lvl' | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $taskPackTarget 'levels')
}
$taskManifest=Get-Content -LiteralPath (Join-Path $taskPack 'manifest.json') -Raw | ConvertFrom-Json
foreach($taskEntry in $taskManifest.files_sha256.PSObject.Properties) {
    $taskCopied=Join-Path $taskPackTarget $taskEntry.Name
    if((Get-FileHash -LiteralPath $taskCopied -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskEntry.Value) { throw "Overlay hash mismatch: $taskCopied" }
}
Write-Host "WeekendAdventure data: $taskOverlay"
Write-Host 'Start -> Weekend Adventure: five consecutive levels. Move: arrows/WASD. Face animal and press/release Space. Escape: menu.'
if($PrepareOnly) { return }
$taskExecutable=Join-Path $PSScriptRoot "bin/Win32/$Configuration/Sven2Bootstrap.exe"
if(!(Test-Path -LiteralPath $taskExecutable)) { throw 'Executable missing. Build separately when coordinated: powershell -File rebuild/Build.ps1' }
$taskArguments=@('--game-dir',(Join-Path $taskRoot 'games/sven2'),'--data-dir',$taskOverlay)
if($NoAudio) { $taskArguments+='--no-audio' }
if($Seconds) { $taskArguments+=@('--seconds',"$Seconds") }
& $taskExecutable @taskArguments
exit $LASTEXITCODE
