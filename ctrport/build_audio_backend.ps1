# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskCmake='C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
& $taskCmake -S "$PSScriptRoot/audio_backend" -B "$taskRoot/build/CTRPort-infra/audio_backend" -G 'Visual Studio 17 2022' -A x64 *> "$taskRoot/build/CTRPort-infra/audio_backend_configure.log"
if($LASTEXITCODE -ne 0){throw 'Audio backend configure failed'}
& $taskCmake --build "$taskRoot/build/CTRPort-infra/audio_backend" --config Release --parallel 1 -- /nodeReuse:false *> "$taskRoot/build/CTRPort-infra/audio_backend_build.log"
if($LASTEXITCODE -ne 0){throw 'Audio backend build failed'}
