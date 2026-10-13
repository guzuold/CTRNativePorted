# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
$taskBuild=Join-Path (Split-Path -Parent $PSScriptRoot) 'build/CTRPort-infra'
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
& (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe') /nologo /EHsc /W4 /WX /std:c++20 "$PSScriptRoot/capture_window.cpp" "/Fe$taskBuild/capture_window.exe" "/Fo$taskBuild/capture_window.obj" d3d11.lib dxgi.lib user32.lib windowsapp.lib
if($LASTEXITCODE){throw 'WGC capture build failed'}

