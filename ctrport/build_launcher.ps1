# SPDX-License-Identifier: GPL-2.0-only
# Builds the native launcher (ctrport/launcher.cpp) as build/CTRPort-dev/CTRNativePorted.exe,
# next to the ctrport.exe it starts. Only Windows system libraries (Win32, GDI+, COM, DWM, Winsock, CNG).
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$out=Join-Path $taskRoot 'build/CTRPort-dev'
New-Item -ItemType Directory -Force $out | Out-Null
$env:PATH="$env:PATH;C:\Program Files (x86)\Microsoft Visual Studio\Installer"
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$cl=Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe'
# The project server (ctrport/online_server) runs inside the launcher when hosting.
$obj=Join-Path $out 'launcher_obj'
New-Item -ItemType Directory -Force $obj | Out-Null
$server=@('json.cpp','crypto.cpp','protocol.cpp','stores.cpp','relay.cpp','server.cpp','upnp.cpp','launcher_online.cpp') | ForEach-Object { Join-Path $PSScriptRoot "online_server/$_" }
& $cl /nologo /c /O2 /W4 /WX /EHsc /utf-8 /std:c++20 /DWIN32_LEAN_AND_MEAN /DNOMINMAX @server "/Fo$obj/" *> "$out/launcher_build.log"
if($LASTEXITCODE -ne 0){Get-Content "$out/launcher_build.log" -Tail 30;throw 'launcher online server build failed'}
$objects=Get-ChildItem $obj -Filter *.obj | ForEach-Object { $_.FullName }
& $cl /nologo /O2 /W4 /WX /EHsc /utf-8 /std:c++20 /DUNICODE /D_UNICODE "$PSScriptRoot/launcher.cpp" @objects "/Fo$out/launcher.obj" "/Fe$out/launcher.exe" /link /SUBSYSTEM:WINDOWS *>> "$out/launcher_build.log"
if($LASTEXITCODE -ne 0){Get-Content "$out/launcher_build.log" -Tail 30;throw 'launcher build failed'}
Copy-Item "$out/launcher.exe" "$out/CTRNativePorted.exe" -Force
Write-Output "launcher build ok: $out/launcher.exe"
