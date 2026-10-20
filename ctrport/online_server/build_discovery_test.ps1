# SPDX-License-Identifier: GPL-2.0-or-later
param([string]$ObjectDirectory='build/release/online_server_obj',[string]$OutputDirectory='build/discovery-test')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if(![IO.Path]::IsPathRooted($ObjectDirectory)){$ObjectDirectory=Join-Path $root $ObjectDirectory}
if(![IO.Path]::IsPathRooted($OutputDirectory)){$OutputDirectory=Join-Path $root $OutputDirectory}
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$lines=cmd /c '"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" >nul && set'
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$cl=Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe'
$objects=Get-ChildItem $ObjectDirectory -Filter *.obj|Where-Object Name -notin @('server_main.obj','server_test.obj','discovery_regression.obj')|ForEach-Object FullName
& $cl /nologo /std:c++20 /EHsc /O2 /W4 /WX /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $PSScriptRoot 'discovery_regression.cpp') $objects "/Fo$OutputDirectory/discovery_regression.obj" "/Fe$OutputDirectory/discovery_regression.exe" /link /SUBSYSTEM:CONSOLE
if($LASTEXITCODE -ne 0){throw 'Discovery regression build failed'}