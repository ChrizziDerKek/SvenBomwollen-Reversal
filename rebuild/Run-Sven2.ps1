param([switch]$Probe,[switch]$NoAudio,[switch]$Tutorial,[string]$DataDirectory,[ValidateRange(0,3600)][int]$Seconds=0,[ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [ValidateSet('native','fsr1','nearest','bilinear')][string]$Upscaler='fsr1',
    [ValidateRange(320,7680)][int]$Width=1280,[ValidateRange(240,4320)][int]$Height=960,
    [ValidateRange(0,2)][float]$Sharpness=0.5,[string]$Capture)
$ErrorActionPreference='Stop'
$taskExecutable=Join-Path $PSScriptRoot "bin/Win32/$Configuration/Sven2Bootstrap.exe"
if(!(Test-Path -LiteralPath $taskExecutable)) { throw 'Build first: powershell -File rebuild/Build.ps1' }
$taskArguments=@('--game-dir',(Join-Path (Split-Path $PSScriptRoot -Parent) 'games/sven2'))
if($Probe) { $taskArguments+='--probe' }
if($Tutorial) { $taskArguments+='--tutorial' }
if($DataDirectory) { $taskArguments+=@('--data-dir',$DataDirectory) }
if($NoAudio) { $taskArguments+='--no-audio' }
if($Seconds) { $taskArguments+=@('--seconds',"$Seconds") }
$taskArguments+=@('--upscaler',$Upscaler,'--output-width',"$Width",'--output-height',"$Height",'--sharpness',$Sharpness.ToString([System.Globalization.CultureInfo]::InvariantCulture))
if($Capture) { $taskArguments+=@('--capture',$Capture) }
& $taskExecutable @taskArguments
exit $LASTEXITCODE
