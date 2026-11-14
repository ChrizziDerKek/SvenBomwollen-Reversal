param(
    [switch]$PrepareOnly,
    [switch]$Probe,
    [switch]$NoAudio,
    [ValidateRange(0,3600)][int]$Seconds=0,
    [ValidateSet('Debug','Release')][string]$Configuration='Debug'
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskOverlay=Join-Path $PSScriptRoot 'runtime/parity-data'
$taskPack=Join-Path $PSScriptRoot 'testpacks/ParityLab'
# Only these two original resource trees are needed. Omit XS.xml so the sole
# catalog package is ParityLab and the existing menu selects it by default.
foreach($taskPackage in @('Sven2','XS')) {
    $taskSource=Join-Path $taskRoot "assets/$taskPackage"
    $taskTarget=Join-Path $taskOverlay $taskPackage
    New-Item -ItemType Directory -Path $taskTarget -Force | Out-Null
    foreach($taskFile in Get-ChildItem -LiteralPath $taskSource -File -Recurse) {
        $taskRelative=$taskFile.FullName.Substring($taskSource.Length).TrimStart('\','/')
        if($taskPackage -eq 'XS' -and $taskRelative -eq 'XS.xml') { continue }
        $taskDestination=Join-Path $taskTarget $taskRelative
        New-Item -ItemType Directory -Path (Split-Path $taskDestination -Parent) -Force | Out-Null
        if(!(Test-Path -LiteralPath $taskDestination)) {
            try { New-Item -ItemType HardLink -Path $taskDestination -Target $taskFile.FullName -ErrorAction Stop | Out-Null }
            catch { Copy-Item -LiteralPath $taskFile.FullName -Destination $taskDestination }
        }
    }
}
# Refuse unexpected descriptors rather than modifying any existing linked XML.
if(Test-Path -LiteralPath (Join-Path $taskOverlay 'XS/XS.xml')) {
    throw 'Unexpected XS catalog in parity-data. Move that overlay aside and rerun.'
}
$taskPackTarget=Join-Path $taskOverlay 'ParityLab'
New-Item -ItemType Directory -Path $taskPackTarget -Force | Out-Null
Copy-Item -Path (Join-Path $taskPack '*') -Destination $taskPackTarget -Recurse -Force
Write-Host "ParityLab data: $taskOverlay"
Write-Host 'Start opens ParityLab; each scenario is a separate episode. Lives contains two levels.'
if($PrepareOnly) { return }
$taskExecutable=Join-Path $PSScriptRoot "bin/Win32/$Configuration/Sven2Bootstrap.exe"
if(!(Test-Path -LiteralPath $taskExecutable)) { throw 'Build first: powershell -File rebuild/Build.ps1' }
$taskArguments=@('--game-dir',(Join-Path $taskRoot 'games/sven2'),'--data-dir',$taskOverlay)
if($Probe) { $taskArguments+='--probe' }
if($NoAudio) { $taskArguments+='--no-audio' }
if($Seconds) { $taskArguments+=@('--seconds',"$Seconds") }
& $taskExecutable @taskArguments
exit $LASTEXITCODE
