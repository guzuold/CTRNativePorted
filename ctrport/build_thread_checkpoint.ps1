$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$lines=cmd /c '"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" >nul && set'
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
& (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe') /nologo /W4 /WX /EHsc /O2 /std:c++17 "$PSScriptRoot/thread_checkpoint.cpp" "/Fo$root/build/CTRPort-infra/thread_checkpoint.obj" "/Fe$root/build/CTRPort-infra/thread_checkpoint.exe" *> "$root/build/CTRPort-infra/thread_checkpoint_build.log"
if($LASTEXITCODE -ne 0){throw 'checkpoint build failed'}
