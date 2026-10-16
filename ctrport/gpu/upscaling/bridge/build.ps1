# SPDX-License-Identifier: MIT
param([string]$Sdk = 'build/dlss-sdk')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/../../../..").Path
$sdkPath = (Resolve-Path (Join-Path $root $Sdk)).Path
$outPath = Join-Path $root 'build/CTRPort-dev'
$vkPath = Join-Path $root 'engine/shadPS4/externals/Vulkan-Headers/include'
$vsPath = 'C:/Program Files/Microsoft Visual Studio/2022/Community'
foreach ($line in (cmd /c "`"$vsPath/VC/Auxiliary/Build/vcvars64.bat`" >nul && set")) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
}
& cl /nologo /O2 /MT /EHsc /std:c++20 /LD /DNOMINMAX /DWIN32_LEAN_AND_MEAN "/I$sdkPath/include" "/I$vkPath" "$PSScriptRoot/shadps4_dlss_bridge.cpp" "/Fo$outPath/ctr_dlss.obj" /link "$sdkPath/lib/Windows_x86_64/x64/nvsdk_ngx_s.lib" advapi32.lib user32.lib legacy_stdio_definitions.lib "/OUT:$outPath/ctr_dlss.dll" "/IMPLIB:$outPath/ctr_dlss.lib"
if ($LASTEXITCODE -ne 0) { throw 'DLSS bridge build failed' }
Copy-Item -LiteralPath "$sdkPath/lib/Windows_x86_64/rel/nvngx_dlss.dll" -Destination "$outPath/nvngx_dlss.dll"
Copy-Item -LiteralPath "$sdkPath/LICENSE.txt" -Destination "$outPath/NVIDIA-DLSS-LICENSE.txt"
