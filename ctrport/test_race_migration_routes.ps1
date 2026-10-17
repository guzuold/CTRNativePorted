# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $vc = 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
    if (!(Test-Path $vc)) { throw 'MSVC 2022 Community toolchain missing' }
    $lines = cmd /c "`"$vc`" >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'MSVC environment failed' }
    foreach ($line in $lines) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }
    $out = 'build/CTRPort-migration-parser'
    New-Item -ItemType Directory -Force $out | Out-Null
    & cl /nologo /O2 /W4 /WX /EHsc /std:c++17 ctrport/test_race_migration_routes.cpp "/Fo$out/routes.obj" "/Fe$out/routes.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Route parser test build failed' }
    & "$out/routes.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Route parser checks failed' }
} finally { Pop-Location }
