# SPDX-License-Identifier: GPL-2.0-only
param([ValidateSet('ctrport.exe','ctrport-host-compiler.exe')][string]$ExecutableName='ctrport.exe')
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1')
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $taskRoot 'build/CTRPort-infra'
New-Item -ItemType Directory -Force $taskBuild | Out-Null
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
& "$PSScriptRoot/build_font_backend.ps1"
& "$PSScriptRoot/build_audio_backend.ps1"
& (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/ml64.exe') /nologo /c "/Fo$taskBuild/abi_probe.obj" "$PSScriptRoot/abi_probe.asm" *> "$taskBuild/assembly.log"
if($LASTEXITCODE -ne 0){throw 'CTRPort assembly failed'}
& (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/ml64.exe') /nologo /c "/Fo$taskBuild/kernel_services.obj" "$PSScriptRoot/kernel_services.asm" *> "$taskBuild/services_assembly.log"
if($LASTEXITCODE -ne 0){throw 'CTRPort service assembly failed'}
& (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/ml64.exe') /nologo /c "/Fo$taskBuild/service_integer_dispatch.obj" "$PSScriptRoot/service_integer_dispatch.asm" *> "$taskBuild/dispatch_assembly.log"
if($LASTEXITCODE -ne 0){throw 'Service dispatch assembly failed'}
# Pin a verified local library before compile/link. Later data-lane publication
# cannot modify this generation; a concurrent change during copy is rejected.
. "$PSScriptRoot/pin_build_library.ps1"
$taskVideoLibrary=Join-Path $taskRoot 'build/CTRPort-data/ctr_video.lib'
$taskVideoPin=Copy-CtrBuildLibrary -Source $taskVideoLibrary -Destination (Join-Path $taskBuild 'ctr_video_pinned.lib')
$taskVideoHash=$taskVideoPin.sha256
$taskCompanionDirectory=Join-Path $taskBuild 'graphics_companions'
New-Item -ItemType Directory -Force $taskCompanionDirectory | Out-Null
$taskCompanionPins=@()
foreach($taskVersion in 22,25,26,27,28,29,30,31,32,33){
 $taskCompanionName="ctr_graphics_companion_v$taskVersion.dll"
 $taskCompanionPins+=Copy-CtrBuildLibrary -Source (Join-Path $taskRoot "build/CTRPort-data/$taskCompanionName") -Destination (Join-Path $taskCompanionDirectory $taskCompanionName)
}

& (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe') /nologo /O2 /W4 /WX /EHsc /bigobj /std:c++17 "$PSScriptRoot/main.cpp" "/Fo$taskBuild/main.obj" "/Fe$taskBuild/$ExecutableName" "$taskBuild/abi_probe.obj" $taskVideoPin.path user32.lib "$taskBuild/audio_backend/Release/ctr_atrac9.lib" "$taskBuild/kernel_services.obj" "$taskBuild/service_integer_dispatch.obj" "$taskBuild/font_backend/Release/ctr_font_backend.lib" "$taskBuild/font_backend/freetype/Release/freetype.lib" *> "$taskBuild/build.log"
if($LASTEXITCODE -ne 0){throw 'CTRPort compilation failed'}
if((Get-FileHash -LiteralPath $taskVideoPin.path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskVideoHash){throw 'Pinned video library changed during infra link'}
$taskExecutable=Join-Path $taskBuild $ExecutableName
$taskManifest=@{schema_version=1; executable=$taskExecutable; executable_sha256=(Get-FileHash -LiteralPath $taskExecutable -Algorithm SHA256).Hash.ToLowerInvariant(); video_library=$taskVideoPin.path; published_video_library=$taskVideoLibrary; video_library_sha256=$taskVideoHash; graphics_companions=$taskCompanionPins; compiler=(Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe'); toolchain=$env:VCToolsVersion; timestamp=[DateTime]::UtcNow.ToString('o')}
$taskManifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $taskBuild ($ExecutableName+'.build.json')) -Encoding UTF8


foreach($taskGeneration in 1,2){
 & (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe') /nologo /LD /O2 /W4 /WX /EHsc /std:c++17 "/DCTR_CONTROL_GENERATION=$taskGeneration" "$PSScriptRoot/service_control_dll.cpp" "/Fo$taskBuild/service_control_$taskGeneration.obj" "/Fe$taskBuild/service_control_$taskGeneration.dll" /link "/IMPLIB:$taskBuild/service_control_$taskGeneration.lib" *> "$taskBuild/service_control_$taskGeneration.log"
 if($LASTEXITCODE -ne 0){throw 'Service control DLL build failed'}
}

foreach($taskGeneration in 1,2){
 & (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe') /nologo /LD /O2 /W4 /WX /EHsc /std:c++17 "$PSScriptRoot/service_write_dll.cpp" "/Fo$taskBuild/service_write_$taskGeneration.obj" "/Fe$taskBuild/service_write_$taskGeneration.dll" /link "/IMPLIB:$taskBuild/service_write_$taskGeneration.lib" *> "$taskBuild/service_write_$taskGeneration.log"
 if($LASTEXITCODE -ne 0){throw 'Write service DLL build failed'}
}

foreach($taskGeneration in 1,2){
 & (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe') /nologo /LD /O2 /W4 /WX /EHsc /std:c++17 "/DCTR_CONTROL_GENERATION=$taskGeneration" "$PSScriptRoot/service_mutex_dll.cpp" "/Fo$taskBuild/service_mutex_$taskGeneration.obj" "/Fe$taskBuild/service_mutex_$taskGeneration.dll" /link "/IMPLIB:$taskBuild/service_mutex_$taskGeneration.lib" *> "$taskBuild/service_mutex_$taskGeneration.log"
 if($LASTEXITCODE -ne 0){throw 'Write service DLL build failed'}
}

foreach($taskGeneration in 1,2){
 & (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe') /nologo /LD /O2 /W4 /WX /EHsc /std:c++17 "/DCTR_CONTROL_GENERATION=$taskGeneration" "$PSScriptRoot/service_nine_dll.cpp" "/Fo$taskBuild/service_nine_$taskGeneration.obj" "/Fe$taskBuild/service_nine_$taskGeneration.dll" /link "/IMPLIB:$taskBuild/service_nine_$taskGeneration.lib" *> "$taskBuild/service_nine_$taskGeneration.log"
 if($LASTEXITCODE -ne 0){throw 'Nine argument service DLL build failed'}
}

& (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe') /nologo /LD /TC /std:c17 /O2 /W4 /WX "$PSScriptRoot/gnm_executor_host_fixture.c" "/Fo$taskBuild/gnm_executor_host_fixture.obj" "/Fe$taskBuild/gnm_executor_host_fixture.dll" /link "/IMPLIB:$taskBuild/gnm_executor_host_fixture.lib" *> "$taskBuild/gnm_executor_host_fixture.log"
if($LASTEXITCODE -ne 0){throw 'Gnm host C ABI fixture build failed'}
