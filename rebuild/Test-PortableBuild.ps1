param([ValidateSet('Debug','Release')][string]$Configuration='Debug')
$ErrorActionPreference='Stop'
$taskRepository=Split-Path $PSScriptRoot -Parent
$taskSource=Join-Path $PSScriptRoot "bin/Win32/$Configuration/Sven2Bootstrap.exe"
if(!(Test-Path -LiteralPath $taskSource)) { throw 'Build the game first.' }
$taskTestRoot=Join-Path $PSScriptRoot ('runtime/portable-check/'+[guid]::NewGuid().ToString('N'))
$taskApplication=Join-Path $taskTestRoot 'relocated/application'
$taskLegacy=Join-Path $taskTestRoot 'legacy'
New-Item -ItemType Directory -Path "$taskApplication/saves","$taskLegacy/nested" -Force | Out-Null
Set-Content -LiteralPath "$taskApplication/saves/existing.tsv" -Value 'newer-save'
Set-Content -LiteralPath "$taskLegacy/existing.tsv" -Value 'older-save'
Set-Content -LiteralPath "$taskLegacy/nested/copied.tsv" -Value 'migrated-save'
$taskVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$taskInstallation=& $taskVswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
$taskMSBuild=Join-Path $taskInstallation 'MSBuild/Current/Bin/MSBuild.exe'
$taskProject=Join-Path $PSScriptRoot 'Sven2Bootstrap/Sven2Bootstrap.vcxproj'
function Deploy-CheckData([bool]$refresh) {
    & $taskMSBuild $taskProject /t:DeployRuntimeData /p:Platform=Win32 "/p:Configuration=$Configuration" "/p:OutDir=$taskApplication/" "/p:SvenLegacySaveRoot=$taskLegacy" "/p:RefreshRuntimeAssets=$refresh" /nologo /v:minimal
    if($LASTEXITCODE -ne 0) { throw 'Runtime deployment failed.' }
}
Deploy-CheckData $false
Copy-Item -LiteralPath $taskSource -Destination "$taskApplication/Sven2Bootstrap.exe"
if((Get-Content -LiteralPath "$taskApplication/saves/existing.tsv" -Raw).Trim() -ne 'newer-save') { throw 'Deployment overwrote an existing save.' }
if((Get-Content -LiteralPath "$taskApplication/saves/nested/copied.tsv" -Raw).Trim() -ne 'migrated-save') { throw 'Nested legacy save was not migrated.' }
$taskCore=Join-Path $taskApplication 'assets/ZWO/Sven2/Sven2.xml'
Add-Content -LiteralPath $taskCore -Value '<!-- Runtime edit retained across builds. -->'
$taskEditedHash=(Get-FileHash -LiteralPath $taskCore).Hash
Deploy-CheckData $false
if((Get-FileHash -LiteralPath $taskCore).Hash -ne $taskEditedHash) { throw 'Deployment overwrote edited runtime assets.' }
Deploy-CheckData $true
if((Get-FileHash -LiteralPath $taskCore).Hash -ne (Get-FileHash -LiteralPath "$taskRepository/assets/ZWO/Sven2/Sven2.xml").Hash) { throw 'Explicit asset refresh did not restore the source asset.' }
if((Get-Content -LiteralPath "$taskApplication/saves/existing.tsv" -Raw).Trim() -ne 'newer-save') { throw 'Asset refresh overwrote saves.' }
$taskCapture=Join-Path $taskTestRoot 'portable-frame.bmp'
Push-Location $env:TEMP
try {
    & "$taskApplication/Sven2Bootstrap.exe" --tutorial --no-audio --seconds 2 --capture $taskCapture
    if($LASTEXITCODE -ne 0) { throw 'Relocated executable failed.' }
} finally { Pop-Location }
if(!(Test-Path -LiteralPath $taskCapture)) { throw 'Relocated FSR renderer did not produce a frame.' }
if(!(Test-Path -LiteralPath "$taskApplication/logs/game.log")) { throw 'Runtime logs escaped the executable directory.' }
foreach($taskShader in @('Fsr1.hlsl','ffx_a.h','ffx_fsr1.h')) {
    if(Test-Path -LiteralPath "$taskApplication/shaders/$taskShader") { throw 'Unexpected loose shader in deployment.' }
}
Write-Output "PASS: relocated FSR game, embedded shaders, asset preservation/refresh and non-destructive save migration. Evidence: $taskTestRoot"
