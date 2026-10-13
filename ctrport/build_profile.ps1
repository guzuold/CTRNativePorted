# SPDX-License-Identifier: GPL-2.0-only
# development build of ctrport.exe: same sources and flags as build.ps1 plus debug
# information (/Zi, /DEBUG) so a sampler can resolve host functions. The assembly
# bridges are assembled from source; the pinned video library and the audio/font
# backends come from the last infra build. Output goes to build/CTRPort-dev and
# never replaces build/CTRPort-infra/ctrport.exe.
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$infra=Join-Path $taskRoot 'build/CTRPort-infra'
$out=Join-Path $taskRoot 'build/CTRPort-dev'
New-Item -ItemType Directory -Force $out | Out-Null
$env:PATH="$env:PATH;C:\Program Files (x86)\Microsoft Visual Studio\Installer"
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$ml=Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/ml64.exe'
foreach($asm in 'abi_probe','kernel_services','service_integer_dispatch'){
 & $ml /nologo /c "/Fo$out/$asm.obj" "$PSScriptRoot/$asm.asm" *> "$out/$asm.assembly.log"
 if($LASTEXITCODE -ne 0){Get-Content "$out/$asm.assembly.log" -Tail 20;throw "Assembly of $asm failed"}
}
$inputs="$out/abi_probe.obj","$infra/ctr_video_pinned.lib","user32.lib","$infra/audio_backend/Release/ctr_atrac9.lib","$out/kernel_services.obj","$out/service_integer_dispatch.obj","$infra/font_backend/Release/ctr_font_backend.lib","$infra/font_backend/freetype/Release/freetype.lib"
foreach($i in $inputs){if($i -ne 'user32.lib' -and -not (Test-Path -LiteralPath $i)){throw "Missing build input $i"}}
& (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe') /nologo /O2 /Zi /W4 /WX /EHsc /bigobj /std:c++17 "$PSScriptRoot/main.cpp" "/Fo$out/main.obj" "/Fd$out/main.pdb" "/Fe$out/ctrport.exe" @inputs /link /DEBUG /OPT:REF /OPT:ICF "/PDB:$out/ctrport.pdb" *> "$out/build.log"
if($LASTEXITCODE -ne 0){Get-Content "$out/build.log" -Tail 20;throw 'ctrport build failed'}
$manifest=@{schema_version=1;purpose='development build with debug information';executable_sha256=(Get-FileHash -LiteralPath "$out/ctrport.exe" -Algorithm SHA256).Hash.ToLowerInvariant();video_library_sha256=(Get-FileHash -LiteralPath "$infra/ctr_video_pinned.lib" -Algorithm SHA256).Hash.ToLowerInvariant();toolchain=$env:VCToolsVersion;timestamp=[DateTime]::UtcNow.ToString('o')}
$manifest | ConvertTo-Json | Set-Content -LiteralPath "$out/ctrport.exe.build.json" -Encoding UTF8
Write-Output "ctrport build ok: $out/ctrport.exe"
