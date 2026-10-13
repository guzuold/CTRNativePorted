# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
if($env:CTR_BUILD_LOCK_HELD -ne '1'){throw 'build-data lock required'}
$taskRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskBuild=Join-Path $taskRoot 'build/CTRPort-data'
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
& cl.exe /nologo /O2 /W4 /WX /EHsc /std:c++17 "$PSScriptRoot/vblank_clock_control.cpp" "/Fo$taskBuild/vblank_clock_control.obj" "/Fe$taskBuild/vblank_clock_control.exe" *> "$taskBuild/vblank_clock_build.log"
if($LASTEXITCODE -ne 0){throw 'Vblank compile failed'}
& "$taskBuild/vblank_clock_control.exe" *> "$taskBuild/vblank_clock_control.log"
if($LASTEXITCODE -ne 0){throw 'Vblank control failed'}
Get-Content "$taskBuild/vblank_clock_control.log"
