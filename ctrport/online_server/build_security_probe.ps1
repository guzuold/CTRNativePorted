# SPDX-License-Identifier: GPL-2.0-or-later
# Rebuild only the current regression probe against already-built server objects.
# Use -Baseline after building b95b24e with build_online_server.ps1 -OutputDirectory build/security-baseline.
param([switch]$Baseline)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$dir=if($Baseline){'build/security-baseline'}else{'build/CTRPort-claude'}
$out=Join-Path $root $dir
$vc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
foreach($line in (cmd /c ('"'+$vc+'" >nul && set'))){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$sources=@('json','crypto','protocol','stores','relay','server','upnp','launcher_online','friend_identity','dht_service','friend_verifier','friend_store','party_lobby','friend_network','dht')
$objects=$sources | ForEach-Object { Join-Path $out "online_server_obj/$_.obj" }
foreach($object in $objects){if(!(Test-Path $object)){throw "Missing object: $object. Build the selected revision first."}}
& cl.exe /nologo /O2 /W4 /WX /EHsc /utf-8 /std:c++20 /DWIN32_LEAN_AND_MEAN /DNOMINMAX "$PSScriptRoot/server_test.cpp" @objects "/Fo$out/security_probe.obj" "/Fe$out/security_probe.exe" /link /SUBSYSTEM:CONSOLE
if($LASTEXITCODE -ne 0){throw 'security probe build failed'}
