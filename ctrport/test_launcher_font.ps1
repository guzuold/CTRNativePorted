# SPDX-License-Identifier: GPL-2.0-only
param(
    [Parameter(Mandatory=$true)][string]$BaseDirectory,
    [Parameter(Mandatory=$true)][string]$PatchDirectory,
    [string]$OutputDirectory='build/CTRPort-room'
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskOut=Join-Path $taskRoot $OutputDirectory
New-Item -ItemType Directory -Force $taskOut | Out-Null
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
& cl.exe /nologo /O2 /W4 /WX /EHsc /utf-8 /std:c++20 "$PSScriptRoot/launcher_font_test.cpp" "/Fo$taskOut/launcher_font_test.obj" "/Fe$taskOut/launcher_font_test.exe"
if($LASTEXITCODE -ne 0){throw 'font test build failed'}
$taskPreview=Join-Path $taskRoot 'runtime/user/launcher-font-preview.png'
& "$taskOut/launcher_font_test.exe" $BaseDirectory $PatchDirectory $taskPreview
if($LASTEXITCODE -ne 0){throw "font test failed: $LASTEXITCODE"}
