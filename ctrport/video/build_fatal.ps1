# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
if($env:CTR_BUILD_LOCK_HELD -ne '1'){throw 'build-data lock required'}
$taskRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskBuild=Join-Path $taskRoot 'build/CTRPort-data'
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
& cl.exe /nologo /c /O2 /W4 /WX /EHsc /std:c++17 "$PSScriptRoot/fatal_adapter.cpp" "/Fo$taskBuild/fatal_adapter.obj" *> "$taskBuild/fatal_build.log"
if($LASTEXITCODE -ne 0){throw 'Fatal adapter compile failed'}
& cl.exe /nologo /O2 /W4 /WX /EHsc /std:c++17 "$PSScriptRoot/fatal_control.cpp" "$taskBuild/fatal_adapter.obj" "/Fo$taskBuild/fatal_control.obj" "/Fe$taskBuild/fatal_control.exe" >> "$taskBuild/fatal_build.log" 2>&1
if($LASTEXITCODE -ne 0){throw 'Fatal control link failed'}
$taskCases=@()
foreach($taskCase in @('assert','unreachable')){
    $taskError="$taskBuild/fatal_$taskCase.stderr.log"
    $taskProcess=Start-Process -FilePath "$taskBuild/fatal_control.exe" -ArgumentList $taskCase -WindowStyle Hidden -PassThru -RedirectStandardError $taskError
    $taskHandle=$taskProcess.Handle # Retain process handle so ExitCode survives exit.
    if(-not $taskProcess.WaitForExit(10000)){Stop-Process -Id $taskProcess.Id;throw 'Fatal adapter failed to terminate'}
    $taskProcess.Refresh()
    $taskText=[IO.File]::ReadAllText($taskError)
    if($taskProcess.ExitCode -ne 86 -or $taskText -notmatch "CTR graphics fatal: $($taskCase -replace '^assert$','assert_fail_impl' -replace '^unreachable$','unreachable_impl')" -or $taskText -match 'ERROR: fatal returned'){throw 'Fatal control failed'}
    $taskCases+=@{case=$taskCase;exit_code=$taskProcess.ExitCode;stderr=$taskText.Trim();returned=$false}
}
function TaskHash($taskPath){([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($taskPath)))).Replace('-','').ToLower()}
$taskFiles=@('ctrport/video/fatal_adapter.cpp','ctrport/video/fatal_control.cpp','ctrport/video/build_fatal.ps1','build/CTRPort-data/fatal_adapter.obj','build/CTRPort-data/fatal_control.exe','build/CTRPort-data/fatal_build.log','build/CTRPort-data/fatal_assert.stderr.log','build/CTRPort-data/fatal_unreachable.stderr.log')
$taskHashes=@{};foreach($taskFile in $taskFiles){$taskHashes[$taskFile]=TaskHash "$taskRoot/$taskFile"}
@{checked_at=[DateTime]::UtcNow.ToString('o');cases=$taskCases;hashes=$taskHashes;scope='Host fatal policy only; no guest service or retail shader executed';R3=$false;retail_shaders_tested=0} | ConvertTo-Json -Depth 5 | Set-Content "$taskRoot/reports/reconstruction/data/shader_fatal_control.json" -Encoding utf8
Write-Output 'Fatal control: 2 child processes terminated with exit86 and exact diagnostic; no continuation'
