# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
New-Item -ItemType Directory -Force (Join-Path $taskRoot 'build/CTRPort-infra') | Out-Null
$taskCmake='C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
& $taskCmake -S "$PSScriptRoot/font_backend" -B "$taskRoot/build/CTRPort-infra/font_backend" -G 'Visual Studio 17 2022' -A x64 *> "$taskRoot/build/CTRPort-infra/font_backend_configure.log"
if($LASTEXITCODE -ne 0){throw 'Font backend configure failed'}
& $taskCmake --build "$taskRoot/build/CTRPort-infra/font_backend" --config Release --target font_backend_control ctr_font_backend --parallel 1 -- /nodeReuse:false *> "$taskRoot/build/CTRPort-infra/font_backend_build.log"
if($LASTEXITCODE -ne 0){throw 'Font backend build failed'}
