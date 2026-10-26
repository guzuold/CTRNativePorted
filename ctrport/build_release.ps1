# SPDX-License-Identifier: GPL-2.0-only
# Builds the whole program from a fresh clone, after tools/bootstrap.ps1, into one folder laid
# out like the release package (launcher.exe at the top, the runtime in build/CTRPort-gpu).
#   powershell -File ctrport/build_release.ps1 [-OutputDirectory build/release-local]
# Steps: audio and font backends, ctr_video.lib, ctrport.exe, ctr_gpu.dll and ctr_hle.dll, ctr_d3d.dll and ctr_shaders.exe,
# ctr_online_server.exe, the launcher, the Visual C++ runtime DLLs, assets and documents.
# ctr_video.lib is compiled here without the GPU control suite of ctrport/video/build.ps1,
# which needs local evidence that is not in this repository; its sources are the same.
# -WithoutDirect3D11: leave out ctr_d3d.dll and ctr_shaders.exe. Updaters of 0.6.1 and older refuse
# a package with paths their closed list lacks; a release built this way carries the newer updater
# first, and the next release can add the files (ctrport/UPDATES.md, "New program files").
param([string]$OutputDirectory = 'build/release-local', [int]$Jobs = 12, [switch]$WithoutDirect3D11)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root $OutputDirectory
if (-not (Test-Path -LiteralPath (Join-Path $root 'engine/shadPS4/src'))) { throw 'engine/shadPS4 missing: run powershell -File tools/bootstrap.ps1 first' }
if (-not (Test-Path -LiteralPath (Join-Path $root 'tools/toolchains/llvm-19.1.1/bin/clang-cl.exe'))) { throw 'LLVM 19.1.1 missing: run powershell -File tools/bootstrap.ps1 first' }
$runtime = Join-Path $out 'build/CTRPort-gpu'
New-Item -ItemType Directory -Force $runtime | Out-Null
$env:PATH = "$env:PATH;C:\Program Files (x86)\Microsoft Visual Studio\Installer"
$vc = 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
foreach ($line in (cmd /c "`"$vc`" >nul && set")) { if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') } }
$bin = Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64'
function Step($name) { Write-Output "== $name" }

Step 'audio and font backends (build/CTRPort-infra)'
& "$PSScriptRoot/build_audio_backend.ps1"
& "$PSScriptRoot/build_font_backend.ps1"
$infra = Join-Path $root 'build/CTRPort-infra'

Step 'ctr_video.lib (build/CTRPort-data)'
$data = Join-Path $root 'build/CTRPort-data'
New-Item -ItemType Directory -Force $data | Out-Null
$vulkan = Join-Path $root 'engine/shadPS4/externals/vulkan-headers/include'
& "$bin/cl.exe" /nologo /c /O2 /W4 /WX /EHsc /std:c++17 /wd4191 "/I$vulkan" "$PSScriptRoot/video/device_control.cpp" "/Fo$data/device_control.obj" *> "$data/release_build.log"
if ($LASTEXITCODE -ne 0) { Get-Content "$data/release_build.log" -Tail 30; throw 'video device compilation failed' }
& "$bin/cl.exe" /nologo /TC /c /O2 /W4 /WX /std:c17 "$PSScriptRoot/video/session_api_c_control.c" "/Fo$data/session_api_c_control.obj" *>> "$data/release_build.log"
if ($LASTEXITCODE -ne 0) { Get-Content "$data/release_build.log" -Tail 30; throw 'video C session compilation failed' }
& "$bin/cl.exe" /nologo /c /O2 /W4 /WX /EHsc /std:c++17 "$PSScriptRoot/video/gnm_init.cpp" "/Fo$data/gnm_init.obj" *>> "$data/release_build.log"
if ($LASTEXITCODE -ne 0) { Get-Content "$data/release_build.log" -Tail 30; throw 'Gnm init compilation failed' }
& "$bin/lib.exe" /nologo "/OUT:$data/ctr_video.lib" "$data/device_control.obj" "$data/session_api_c_control.obj" "$data/gnm_init.obj" *>> "$data/release_build.log"
if ($LASTEXITCODE -ne 0) { throw 'ctr_video.lib failed' }

Step 'ctrport.exe'
$obj = Join-Path $out 'obj'
New-Item -ItemType Directory -Force $obj | Out-Null
foreach ($asm in 'abi_probe', 'kernel_services', 'service_integer_dispatch') {
    & "$bin/ml64.exe" /nologo /c "/Fo$obj/$asm.obj" "$PSScriptRoot/$asm.asm" *> "$obj/$asm.log"
    if ($LASTEXITCODE -ne 0) { Get-Content "$obj/$asm.log" -Tail 20; throw "$asm assembly failed" }
}
# The in-game PC Settings menu: guest call transport and the settings hooks (ctrport/settings).
& "$bin/ml64.exe" /nologo /c "/Fo$obj/settings_transport.obj" "$PSScriptRoot/settings/menu_bridge/transport.asm" *> "$obj/settings_transport.log"
if ($LASTEXITCODE -ne 0) { Get-Content "$obj/settings_transport.log" -Tail 20; throw 'settings transport assembly failed' }
$settings = 'settings/menu_bridge/bridge.cpp', 'settings/geometry_controls/distance_hook.cpp', 'settings/quality_controls/texture_hook.cpp', 'settings/quality_controls/world_hook.cpp', 'settings/shadow_quality/hook.cpp', 'online_server/json.cpp' | ForEach-Object { Join-Path $PSScriptRoot $_ }
& "$bin/cl.exe" /nologo /O2 /W4 /WX /EHsc /bigobj /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS "$PSScriptRoot/main.cpp" @settings "$obj/settings_transport.obj" "/Fo$obj/" "/Fe$runtime/ctrport.exe" "$obj/abi_probe.obj" "$data/ctr_video.lib" user32.lib "$infra/audio_backend/Release/ctr_atrac9.lib" "$obj/kernel_services.obj" "$obj/service_integer_dispatch.obj" "$infra/font_backend/Release/ctr_font_backend.lib" "$infra/font_backend/freetype/Release/freetype.lib" *> "$obj/ctrport_build.log"
if ($LASTEXITCODE -ne 0) { Get-Content "$obj/ctrport_build.log" -Tail 30; throw 'ctrport.exe build failed' }

Step 'ctr_gpu.dll and ctr_hle.dll (build/CTRPort-gpu)'
& "$PSScriptRoot/gpu/build.ps1" -Jobs $Jobs
foreach ($dll in 'ctr_gpu.dll', 'ctr_hle.dll') { Copy-Item (Join-Path $root "build/CTRPort-gpu/$dll") $runtime -Force }

Step 'ctr_d3d.dll and ctr_shaders.exe (build/CTRPort-d3d): the Direct3D 11 renderer and its shader translator'
& "$PSScriptRoot/d3d/build.ps1" -Jobs $Jobs -DllOnly
if ($WithoutDirect3D11) {
    foreach ($file in 'ctr_d3d.dll', 'ctr_shaders.exe') { Remove-Item -LiteralPath (Join-Path $runtime $file) -ErrorAction SilentlyContinue }
} else {
    foreach ($file in 'ctr_d3d.dll', 'ctr_shaders.exe') { Copy-Item (Join-Path $root "build/CTRPort-d3d/$file") $runtime -Force }
}

Step 'ctr_online_server.exe'
& "$PSScriptRoot/online_server/build_online_server.ps1" -OutputDirectory "$OutputDirectory/obj/online_server"
Copy-Item (Join-Path $out 'obj/online_server/ctr_online_server.exe') $runtime -Force

Step 'launcher'
& "$PSScriptRoot/build_launcher.ps1" -OutputDirectory "$OutputDirectory/obj/launcher"
foreach ($file in 'launcher.exe', 'launcher_update.ps1', 'update-version.json') { Copy-Item (Join-Path $out "obj/launcher/$file") $out -Force }

Step 'Visual C++ runtime, assets and documents'
$crt = Get-ChildItem (Join-Path $env:VCToolsRedistDir 'x64') -Directory -Filter 'Microsoft.VC*.CRT' | Select-Object -First 1
if (-not $crt) { throw 'Visual C++ redistributable DLLs not found' }
foreach ($dll in 'msvcp140.dll', 'msvcp140_atomic_wait.dll', 'vcruntime140.dll', 'vcruntime140_1.dll') { Copy-Item (Join-Path $crt.FullName $dll) $runtime -Force }
New-Item -ItemType Directory -Force (Join-Path $out 'assets/launcher'), (Join-Path $out 'assets/xbox-default'), (Join-Path $out 'licenses') | Out-Null
Copy-Item "$PSScriptRoot/assets/launcher/logo.png", "$PSScriptRoot/assets/launcher/README.md" (Join-Path $out 'assets/launcher') -Force
Copy-Item "$PSScriptRoot/assets/xbox-default/*" (Join-Path $out 'assets/xbox-default') -Force
Copy-Item "$PSScriptRoot/dht/LICENCE" (Join-Path $out 'licenses/jech-dht-LICENCE.txt') -Force
if (-not $WithoutDirect3D11) {  # MIT code in ctr_d3d.dll / ctr_shaders.exe
    Copy-Item "$PSScriptRoot/d3d/LICENSE.FrankHUN88453" (Join-Path $out 'licenses/FrankHUN88453-CTR-NitroFueled-PC-LICENSE.txt') -Force
    Copy-Item "$PSScriptRoot/d3d/fsr/LICENSE.AMD-FidelityFX" (Join-Path $out 'licenses/AMD-FidelityFX-FSR1-LICENSE.txt') -Force
    Copy-Item "$root/engine/shadPS4/externals/sdl3/LICENSE.txt" (Join-Path $out 'licenses/SDL3-LICENSE.txt') -Force  # SDL in ctr_d3d.dll
}
foreach ($doc in 'README.md', 'LICENSE', 'CHANGELOG.md', 'FIRST_UPDATE.md') { Copy-Item (Join-Path $root $doc) $out -Force }
Copy-Item "$PSScriptRoot/settings/README.md" (Join-Path $out 'PC_SETTINGS.md') -Force
foreach ($doc in 'ctrport/UPDATES.md', 'ctrport/MODS.md', 'ctrport/gpu/UPSCALING.md', 'ctrport/online_server/INVITATIONS.md', 'tools/online/HOST_MIGRATION.md', 'ctrport/LOCAL_BACKUPS.md') {
    $path = Join-Path $root $doc
    if (Test-Path -LiteralPath $path) { Copy-Item $path $out -Force }
}
Step 'package paths'
# Installed launchers apply only the updater's closed list of paths (launcher_update.ps1,
# Assert-PackagePath): any other file at the top makes Check updates refuse the whole ZIP.
# Child scope: the updater's -Root parameter would otherwise replace $root here.
& {
    $base = (Resolve-Path -LiteralPath $out).Path
    . "$PSScriptRoot/launcher_update.ps1" -Mode Library
    Get-ChildItem -LiteralPath $base -Recurse -File | Where-Object { $_.FullName -notlike "$base\obj\*" } |
        ForEach-Object { Assert-PackagePath $_.FullName.Substring($base.Length + 1).Replace('\', '/') }
}
Write-Output "release build ok: $out (start launcher.exe)"
