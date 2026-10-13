# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
if($env:CTR_BUILD_LOCK_HELD -ne '1'){throw 'build-data lock required'}
$taskRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskBuild=Join-Path $taskRoot 'build/CTRPort-data'
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
& cl.exe /nologo /c /O2 /W4 /WX /EHsc /std:c++17 "$PSScriptRoot/thread_name_adapter.cpp" "/Fo$taskBuild/thread_name_adapter.obj" *> "$taskBuild/thread_name_build.log"
if($LASTEXITCODE -ne 0){throw 'Thread name compile failed'}
& cl.exe /nologo /O2 /W4 /WX /EHsc /std:c++17 "$PSScriptRoot/thread_name_control.cpp" "$taskBuild/thread_name_adapter.obj" "/Fo$taskBuild/thread_name_control.obj" "/Fe$taskBuild/thread_name_control.exe" >> "$taskBuild/thread_name_build.log" 2>&1
if($LASTEXITCODE -ne 0){throw 'Thread name control link failed'}
& "$taskBuild/thread_name_control.exe" *> "$taskBuild/thread_name_control.log"
if($LASTEXITCODE -ne 0){throw 'Thread name control failed'}
function TaskHash($taskPath){([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($taskPath)))).Replace('-','').ToLower()}
$taskFiles=@('ctrport/video/thread_name_adapter.cpp','ctrport/video/thread_name_control.cpp','ctrport/video/build_host.ps1','build/CTRPort-data/thread_name_adapter.obj','build/CTRPort-data/thread_name_control.exe','build/CTRPort-data/thread_name_build.log','build/CTRPort-data/thread_name_control.log')
$taskHashes=@{};foreach($taskFile in $taskFiles){$taskHashes[$taskFile]=TaskHash "$taskRoot/$taskFile"}
@{checked_at=[DateTime]::UtcNow.ToString('o');passed=$true;cases=@('empty','ascii','utf8','worker-isolation');hashes=$taskHashes;scope='Windows host description only; guest pthread name binding absent';R3=$false;retail_shaders_tested=0} | ConvertTo-Json -Depth 5 | Set-Content "$taskRoot/reports/reconstruction/data/shader_host_control.json" -Encoding utf8
Get-Content "$taskBuild/thread_name_control.log"
