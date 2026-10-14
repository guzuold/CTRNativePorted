param([switch]$SkipGpu)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1')
if(-not $SkipGpu){ & "$PSScriptRoot/../../ctrport/gpu/build.ps1" }
& "$PSScriptRoot/../../ctrport/build_profile.ps1"
& "$PSScriptRoot/../../ctrport/build_launcher.ps1"
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out=Join-Path $root 'build/CTRPort-dev'
& (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe') /nologo /EHsc /std:c++17 /W4 /WX "$PSScriptRoot/control_test.cpp" "/Fo$out/prompt_control.obj" "/Fe$out/prompt_control.exe"
if($LASTEXITCODE){throw 'Prompt controls compilation failed'}
& "$out/prompt_control.exe" "$root/dump/CUSA14876"
if($LASTEXITCODE){throw 'Prompt controls failed'}
