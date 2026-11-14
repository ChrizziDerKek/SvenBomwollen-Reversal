param([switch]$Probe,[switch]$NoAudio,[switch]$Tutorial,[string]$DataDirectory,[ValidateRange(0,3600)][int]$Seconds=0,[ValidateSet('Debug','Release')][string]$Configuration='Debug')
$ErrorActionPreference='Stop'
$taskExecutable=Join-Path $PSScriptRoot "bin/Win32/$Configuration/Sven2Bootstrap.exe"
if(!(Test-Path -LiteralPath $taskExecutable)) { throw 'Build first: powershell -File rebuild/Build.ps1' }
$taskArguments=@('--game-dir',(Join-Path (Split-Path $PSScriptRoot -Parent) 'games/sven2'))
if($Probe) { $taskArguments+='--probe' }
if($Tutorial) { $taskArguments+='--tutorial' }
if($DataDirectory) { $taskArguments+=@('--data-dir',$DataDirectory) }
if($NoAudio) { $taskArguments+='--no-audio' }
if($Seconds) { $taskArguments+=@('--seconds',"$Seconds") }
& $taskExecutable @taskArguments
exit $LASTEXITCODE
