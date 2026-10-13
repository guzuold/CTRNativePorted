# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
if($env:CTR_BUILD_LOCK_HELD -ne '1'){throw 'build-data lock required'}
$taskBuild=Join-Path $PSScriptRoot '../../build/CTRPort-data'
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
& cl.exe /nologo /std:c++17 /EHsc /W4 /WX "$PSScriptRoot/srt_provenance_control.cpp" "/Fo$taskBuild/srt_provenance_control.obj" "/Fe$taskBuild/srt_provenance_control.exe" *> "$taskBuild/srt_provenance_compile.log"
if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/srt_provenance_compile.log";exit $LASTEXITCODE}
& "$taskBuild/srt_provenance_control.exe" *> "$taskBuild/srt_provenance_control.log"
exit $LASTEXITCODE
