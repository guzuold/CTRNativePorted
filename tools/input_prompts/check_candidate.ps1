# Link reviewable candidates without replacing binaries used by running games.
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$vs='C:/Program Files/Microsoft Visual Studio/2022/Community'
foreach($line in (cmd /c "`"$vs/VC/Auxiliary/Build/vcvars64.bat`" >nul && set")) {
 if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}
}
$out=Join-Path $root 'build/CTRPort-claude'
$infra=Join-Path $root 'build/CTRPort-infra'
$cl=Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe'
& $cl /nologo /EHsc /std:c++17 /W4 /WX "$PSScriptRoot/control_test.cpp" "/Fo$out/prompt_control.obj" "/Fe$out/prompt_control.exe"
if($LASTEXITCODE){throw 'Control compilation failed'}
& "$out/prompt_control.exe" "$root/../Staging/NitroFueled_GameFiles/CUSA14876"
if($LASTEXITCODE){throw 'Control failed'}
& $cl /nologo "$out/main.obj" "$out/abi_probe.obj" "$infra/ctr_video_pinned.lib" user32.lib "$infra/audio_backend/Release/ctr_atrac9.lib" "$out/kernel_services.obj" "$out/service_integer_dispatch.obj" "$infra/font_backend/Release/ctr_font_backend.lib" "$infra/font_backend/freetype/Release/freetype.lib" "/Fe$out/ctrport_xbox_candidate.exe" /link /DEBUG /OPT:REF /OPT:ICF "/PDB:$out/ctrport_xbox_candidate.pdb"
if($LASTEXITCODE){throw 'Runtime candidate link failed'}
& $cl /nologo /O2 /W4 /WX /EHsc /utf-8 /std:c++20 /DUNICODE /D_UNICODE "$root/ctrport/launcher.cpp" "/Fo$out/launcher_xbox_candidate.obj" "/Fe$out/CTRNativePorted_xbox_candidate.exe" /link /SUBSYSTEM:WINDOWS
if($LASTEXITCODE){throw 'Launcher candidate failed'}
Push-Location "$root/build/CTRPort-gpu"
try {
 & "$root/tools/toolchains/llvm-19.1.1/bin/lld-link.exe" /nologo '@CMakeFiles/ctr_gpu.rsp' /out:ctr_gpu_xbox_candidate.dll /implib:ctr_gpu_xbox_candidate.lib /pdb:ctr_gpu_xbox_candidate.pdb /dll /machine:x64 /debug
 if($LASTEXITCODE){throw 'GPU candidate link failed'}
} finally {Pop-Location}
