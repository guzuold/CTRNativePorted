# SPDX-License-Identifier: GPL-2.0-only
# Builds the native launcher (ctrport/launcher.cpp) as build/CTRPort-dev/CTRNativePorted.exe,
# next to the ctrport.exe it starts. Only Windows system libraries (Win32, GDI+, COM, DWM).
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$out=Join-Path $taskRoot 'build/CTRPort-dev'
New-Item -ItemType Directory -Force $out | Out-Null
$env:PATH="$env:PATH;C:\Program Files (x86)\Microsoft Visual Studio\Installer"
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
& (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe') /nologo /O2 /W4 /WX /EHsc /utf-8 /std:c++20 /DUNICODE /D_UNICODE "$PSScriptRoot/launcher.cpp" "/Fo$out/launcher.obj" "/Fe$out/CTRNativePorted.exe" /link /SUBSYSTEM:WINDOWS *> "$out/launcher_build.log"
if($LASTEXITCODE -ne 0){Get-Content "$out/launcher_build.log" -Tail 30;throw 'launcher build failed'}
Write-Output "launcher build ok: $out/CTRNativePorted.exe"
