# SPDX-License-Identifier: GPL-2.0-or-later
# Builds build/CTRPort-d3d/ctr_d3d.dll and ctr_hle.dll with clang-cl (tools/toolchains/llvm-19.1.1),
# Visual Studio's CMake and Ninja. Run under the build-claude lock:
#   powershell -File ctrport/d3d/build.ps1
# -DllOnly: only ctr_d3d.dll (a release build makes ctr_hle.dll with ctrport/gpu/build.ps1).
param([int]$Jobs = 12, [switch]$Reconfigure, [switch]$DllOnly)
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
$build = Join-Path $root 'build/CTRPort-d3d'
if (-not (Test-Path (Join-Path $build 'build.ninja')) -or $Reconfigure) {
    cmake -S $PSScriptRoot -B $build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo `
        -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_LINKER=lld-link
    if ($LASTEXITCODE -ne 0) { throw 'ctr_d3d configure failed' }
}
if ($DllOnly) { cmake --build $build --target ctr_d3d -j $Jobs }
else { cmake --build $build --target ctr_d3d ctr_hle -j $Jobs }
if ($LASTEXITCODE -ne 0) { throw 'ctr_d3d build failed' }
Write-Output "ctr_d3d build ok: $build/ctr_d3d.dll"
