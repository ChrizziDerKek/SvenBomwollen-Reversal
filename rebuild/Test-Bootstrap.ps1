param([ValidateSet('Debug','Release')][string]$Configuration='Debug')
$ErrorActionPreference='Stop'
$taskExecutable=Join-Path $PSScriptRoot "bin/Win32/$Configuration/Sven2Bootstrap.exe"
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskGame=Join-Path $taskRoot 'games/sven2'
& $taskExecutable --probe --game-dir $taskGame
if($LASTEXITCODE -ne 0) { throw 'Original-engine and folder-data probe failed.' }
& $taskExecutable --probe --game-dir (Join-Path $taskRoot 'games/sven004')
if($LASTEXITCODE -ne 1) { throw 'The incompatible 004 engine must be rejected before loading it.' }
& $taskExecutable --probe --game-dir (Join-Path $PSScriptRoot 'missing-game')
if($LASTEXITCODE -ne 1) { throw 'Missing game directory must produce an actionable error.' }
& (Join-Path $PSScriptRoot 'Build.ps1') -Configuration $Configuration -Checks
if($LASTEXITCODE -ne 0) { throw 'Remake checks build failed.' }
$taskChecks=Join-Path $PSScriptRoot "bin/Win32/$Configuration/Sven2Checks.exe"
$taskCheckLog=Join-Path $PSScriptRoot "runtime/checks-$Configuration.log"
& $taskChecks | Tee-Object -FilePath $taskCheckLog
if($LASTEXITCODE -ne 0) { throw 'Remake checks failed.' }
Write-Output 'PASS: folder data, native engine, isolated remake checks, incompatible-build rejection.'
exit 0
