param([ValidateSet('Debug','Release')][string]$Configuration='Debug',[switch]$Checks,[switch]$RefreshAssets)
$ErrorActionPreference='Stop'
$taskVswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$taskInstallation=if(Test-Path -LiteralPath $taskVswhere) { & $taskVswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath }
if(!$taskInstallation) { $taskInstallation=Join-Path $env:ProgramFiles 'Microsoft Visual Studio/2022/Community' }
$taskMSBuild=Join-Path $taskInstallation 'MSBuild/Current/Bin/MSBuild.exe'
if(!(Test-Path -LiteralPath $taskMSBuild)) { throw 'Install Visual Studio C++ desktop tools (v143 and a Windows SDK), or build SvenRebuild.sln from Visual Studio.' }
$taskProject=if($Checks) { Join-Path $PSScriptRoot 'tests/Checks.vcxproj' } else { Join-Path $PSScriptRoot 'SvenRebuild.sln' }
$taskPlatform=if($Checks) { 'Win32' } else { 'x86' }
& $taskMSBuild $taskProject /m "/p:Configuration=$Configuration" "/p:Platform=$taskPlatform" "/p:RefreshRuntimeAssets=$($RefreshAssets.IsPresent)" /v:minimal
exit $LASTEXITCODE
