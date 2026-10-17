# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $root
try {
    $vc = 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
    $lines = cmd /c "`"$vc`" >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'MSVC environment failed' }
    foreach ($line in $lines) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }
    New-Item -ItemType Directory -Force build/CTRPort-migration-probe | Out-Null
    & (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe') /nologo /W4 /WX /EHsc /std:c++17 tools/online/runtime_host_loss_trace.cpp /Fobuild/CTRPort-migration-probe/runtime_host_loss_trace.obj /Febuild/CTRPort-migration-probe/runtime_host_loss_trace.exe
    if ($LASTEXITCODE -ne 0) { throw 'Host-loss trace build failed' }
} finally { Pop-Location }
