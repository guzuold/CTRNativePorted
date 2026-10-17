# SPDX-License-Identifier: GPL-2.0-or-later
# Builds the C++ project server as build/CTRPort-dev/ctr_online_server.exe and its test
# ctr_online_server_test.exe. Only Windows system libraries (Winsock, CNG).
param([string]$OutputDirectory = 'build/CTRPort-dev')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out=Join-Path $taskRoot $OutputDirectory
$obj=Join-Path $out 'online_server_obj'
New-Item -ItemType Directory -Force $obj | Out-Null
$env:PATH="$env:PATH;C:\Program Files (x86)\Microsoft Visual Studio\Installer"
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$cl=Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe'
$common=@('/nologo','/O2','/W4','/WX','/EHsc','/utf-8','/std:c++20','/DWIN32_LEAN_AND_MEAN','/DNOMINMAX',"/Fo$obj/")
$dhtDir=Join-Path (Split-Path -Parent $PSScriptRoot) 'dht'
& $cl /nologo /c /O2 /W3 /wd4244 /wd4267 /D_CRT_SECURE_NO_WARNINGS /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $dhtDir 'dht.c') "/Fo$obj/dht.obj" *> "$out/online_server_build.log"
if($LASTEXITCODE -ne 0){Get-Content "$out/online_server_build.log" -Tail 40;throw 'dht.c build failed'}
$sources=@('json.cpp','crypto.cpp','protocol.cpp','stores.cpp','relay.cpp','server.cpp','upnp.cpp','launcher_online.cpp','friend_identity.cpp','dht_service.cpp','friend_verifier.cpp','friend_store.cpp','party_lobby.cpp') | ForEach-Object { Join-Path $PSScriptRoot $_ }
& $cl @common @sources (Join-Path $obj 'dht.obj') (Join-Path $PSScriptRoot 'server_main.cpp') "/Fe$out/ctr_online_server.exe" /link /SUBSYSTEM:CONSOLE *> "$out/online_server_build.log"
if($LASTEXITCODE -ne 0){Get-Content "$out/online_server_build.log" -Tail 40;throw 'online server build failed'}
& $cl @common @sources (Join-Path $obj 'dht.obj') (Join-Path $PSScriptRoot 'server_test.cpp') "/Fe$out/ctr_online_server_test.exe" /link /SUBSYSTEM:CONSOLE *> "$out/online_server_test_build.log"
if($LASTEXITCODE -ne 0){Get-Content "$out/online_server_test_build.log" -Tail 40;throw 'online server test build failed'}
Write-Output "online server build ok: $out/ctr_online_server.exe"
