# SPDX-License-Identifier: GPL-2.0-or-later
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out=Join-Path $root 'build/security-policy'
New-Item -ItemType Directory -Force $out | Out-Null
$vc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
foreach($line in (cmd /c ('"'+$vc+'" >nul && set'))){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
& cl.exe /nologo /O2 /W4 /WX /wd4456 /D_CRT_SECURE_NO_WARNINGS /EHsc /std:c++20 /DWIN32_LEAN_AND_MEAN /DNOMINMAX "$PSScriptRoot/incoming_policy_test.cpp" "/Fo$out/" "/Fe$out/incoming_policy_test.exe" /link ws2_32.lib
if($LASTEXITCODE -ne 0){throw 'incoming policy build failed'}
