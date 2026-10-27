# SPDX-License-Identifier: GPL-2.0-or-later
# Builds build/CTRPort-gpu/ctr_gpu.dll with clang-cl (tools/toolchains/llvm-19.1.1),
# Visual Studio's CMake and Ninja:  powershell -File ctrport/gpu/build.ps1
param([int]$Jobs = 12, [switch]$Reconfigure)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$vs = 'C:/Program Files/Microsoft Visual Studio/2022/Community'
$env:PATH = "$env:PATH;C:\Program Files (x86)\Microsoft Visual Studio\Installer"
foreach ($line in (cmd /c "`"$vs/VC/Auxiliary/Build/vcvars64.bat`" >nul && set")) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
}
$llvm = Join-Path $root 'tools/toolchains/llvm-19.1.1/bin'
if (-not (Test-Path (Join-Path $llvm 'clang-cl.exe'))) { throw "clang-cl missing in $llvm (see tools/toolchains/README.md)" }
$env:PATH = "$llvm;$vs/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin;$vs/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja;$env:PATH"
$build = Join-Path $root 'build/CTRPort-gpu'

# FireBurn/FSR-Vulkan (MIT, engine/fsr-vulkan from tools/bootstrap.ps1): AMD FSR 3.1 and frame generation. Its generated
# shader headers pass Windows' 260-character path limit under a deep checkout, so it is
# configured and built through a short junction in %TEMP%; its static libraries are copied here.
$fsrSource = Join-Path $root 'engine/fsr-vulkan'
if (-not (Test-Path (Join-Path $fsrSource 'include/ffx_vk_portable.h'))) { throw 'engine/fsr-vulkan missing: run powershell -File tools/bootstrap.ps1' }
$fsrLibs = Join-Path $build 'fsr-vulkan'
New-Item -ItemType Directory -Force $fsrLibs | Out-Null
$tag = ([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($root.ToLowerInvariant())))).Replace('-', '').Substring(0, 8)
$fsrShort = Join-Path $env:TEMP "ctrfsr-$tag"
$fsrBuild = Join-Path $env:TEMP "ctrfsrb-$tag"
if (-not (Test-Path $fsrShort)) { New-Item -ItemType Junction -Path $fsrShort -Target $fsrSource | Out-Null }
if ((Get-Item $fsrShort).Target -ne $fsrSource -and (Get-Item $fsrShort).Target -notcontains $fsrSource) { throw "$fsrShort does not point to $fsrSource" }
# Local fixes and the HUD-less frame-generation source (patches/fsr-vulkan/README.md).
foreach ($patch in Get-ChildItem (Join-Path $PSScriptRoot 'patches/fsr-vulkan') -Filter '*.patch' | Sort-Object Name) {
    cmd /c "git -C `"$fsrShort`" -c core.longpaths=true apply --reverse --check `"$($patch.FullName)`" 2>nul"
    if ($LASTEXITCODE -eq 0) { continue } # already applied
    cmd /c "git -C `"$fsrShort`" -c core.longpaths=true apply `"$($patch.FullName)`""
    if ($LASTEXITCODE -ne 0) { throw "FSR-Vulkan patch $($patch.Name) does not apply" }
    Write-Host "      engine/fsr-vulkan: applied $($patch.Name)"
}
# FindVulkan needs vulkan-1.lib: an import library of the system loader's exports.
$vulkanLib = Join-Path $fsrLibs 'vulkan-1.lib'
if (-not (Test-Path $vulkanLib)) {
    $exports = & dumpbin /nologo /exports (Join-Path $env:SystemRoot 'System32/vulkan-1.dll') |
        Select-String '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\S+)' | ForEach-Object { $_.Matches[0].Groups[1].Value }
    if (-not $exports) { throw 'vulkan-1.dll exports not found (Vulkan loader missing?)' }
    (@('EXPORTS') + $exports) | Set-Content (Join-Path $fsrLibs 'vulkan-1.def') -Encoding ascii
    & lib /nologo "/def:$fsrLibs/vulkan-1.def" /machine:x64 "/out:$vulkanLib" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'vulkan-1.lib could not be made' }
}
if (-not (Test-Path (Join-Path $fsrBuild 'build.ninja')) -or $Reconfigure) {
    cmake -S $fsrShort -B $fsrBuild -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl `
        -DCMAKE_LINKER=lld-link -DFFX_VK_PORTABLE_BUILD_TESTS=OFF -DFFX_VK_PORTABLE_BUILD_PROBE=OFF `
        "-DCMAKE_CXX_FLAGS=/EHsc -Wno-c++11-narrowing" "-DVulkan_INCLUDE_DIR=$root/engine/shadPS4/externals/vulkan-headers/include" `
        "-DVulkan_LIBRARY=$vulkanLib"
    if ($LASTEXITCODE -ne 0) { throw 'FSR-Vulkan configure failed' }
}
cmake --build $fsrBuild --target ffx_vulkan_portable ffx_vulkan_fsr3_vk_framegeneration_3_1_6 -j $Jobs
if ($LASTEXITCODE -ne 0) { throw 'FSR-Vulkan build failed' }
foreach ($lib in 'ffx_vulkan_portable', 'ffx_vulkan_fsr3_host_1_1_4', 'ffx_vulkan_fsr3_vk_backend_1_1_4', 'ffx_vulkan_fsr3_vk_framegeneration_3_1_6') {
    Copy-Item (Join-Path $fsrBuild "$lib.lib") $fsrLibs -Force
}
if (-not (Test-Path (Join-Path $build 'build.ninja')) -or $Reconfigure) {
    cmake -S $PSScriptRoot -B $build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo `
        -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_LINKER=lld-link "-DCTR_FSR_VULKAN_LIBS=$fsrLibs"
    if ($LASTEXITCODE -ne 0) { throw 'ctr_gpu configure failed' }
}
cmake --build $build --target ctr_gpu ctr_hle ctr_hle_online_test -j $Jobs -- -k 0
if ($LASTEXITCODE -ne 0) { throw 'ctr_gpu build failed' }
Write-Output "ctr_gpu build ok: $build/ctr_gpu.dll"
