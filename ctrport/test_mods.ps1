# SPDX-License-Identifier: GPL-2.0-only
# Builds and runs ctrport/mods_test.cpp (mods folder, mods.json, patch XML) as C++17 like
# ctrport.exe and as C++20 like the launcher. Synthetic files only.
param([string]$OutputDirectory = 'build/CTRPort-mods-test')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$out = Join-Path $taskRoot $OutputDirectory
New-Item -ItemType Directory -Force $out | Out-Null
$env:PATH = "$env:PATH;C:\Program Files (x86)\Microsoft Visual Studio\Installer"
$taskVc = 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
foreach ($line in (cmd /c "`"$taskVc`" >nul && set")) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
}
$cl = Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe'
foreach ($std in 'c++17', 'c++20') {
    $exe = Join-Path $out "mods_test_$($std -replace '\+', 'p').exe"
    & $cl /nologo /O2 /W4 /WX /EHsc /utf-8 "/std:$std" "$PSScriptRoot/mods_test.cpp" "/Fo$out/" "/Fe$exe" bcrypt.lib *> "$out/build_$std.log"
    if ($LASTEXITCODE -ne 0) { Get-Content "$out/build_$std.log" -Tail 30; throw "mods test build failed ($std)" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "mods test failed ($std)" }
}
# GameBanana downloads (launcher only, C++20): synthetic API answers and extracted folders.
$exe = Join-Path $out 'gamebanana_test.exe'
& $cl /nologo /O2 /W4 /WX /EHsc /utf-8 /std:c++20 "$PSScriptRoot/gamebanana_test.cpp" "$PSScriptRoot/online_server/json.cpp" "/Fo$out/" "/Fe$exe" *> "$out/build_gamebanana.log"
if ($LASTEXITCODE -ne 0) { Get-Content "$out/build_gamebanana.log" -Tail 30; throw 'gamebanana test build failed' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'gamebanana test failed' }
