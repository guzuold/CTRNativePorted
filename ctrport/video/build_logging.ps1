# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
if($env:CTR_BUILD_LOCK_HELD -ne '1'){throw 'build-data lock required'}
$taskRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskBuild=Join-Path $taskRoot 'build/CTRPort-data'
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$taskUp="$taskRoot/engine/shadPS4"
$taskClassPath="$taskUp/src/common/logging/classes.h"
$taskClassText=[IO.File]::ReadAllText($taskClassPath)
$taskClassMatches=[regex]::Matches($taskClassText,'constexpr auto \w+ = "([^"]+)";')
$taskClassMatches | ForEach-Object {'"'+$_.Groups[1].Value+'",'} | Set-Content "$taskBuild/logging_classes.inc" -Encoding ascii
$taskCompiler="$taskRoot/tools/toolchains/llvm-19.1.1/bin/clang-cl.exe"
$taskFlags=@('/nologo','/c','/O2','/W4','/WX','/EHsc','/std:c++latest',"/I$taskBuild","/I$taskUp/externals/spdlog/include","/I$taskUp/externals/fmt/include")
function InvokeTaskNative($taskExe, $taskArgs, $taskLabel) {
    $taskOut="$taskBuild/logging_$taskLabel.stdout.log"
    $taskErr="$taskBuild/logging_$taskLabel.stderr.log"
    $taskQuoted=@($taskArgs | ForEach-Object {'"'+$_+'"'})
    $taskStart=@{FilePath=$taskExe;PassThru=$true;WindowStyle='Hidden';RedirectStandardOutput=$taskOut;RedirectStandardError=$taskErr}
    if($taskQuoted.Count -gt 0){$taskStart.ArgumentList=$taskQuoted}
    $taskProcess=Start-Process @taskStart
    $taskHandle=$taskProcess.Handle
    if(-not $taskProcess.WaitForExit(60000)) {
        $taskProcess.Kill(); $taskProcess.WaitForExit()
        throw "Native stage timed out: $taskLabel (owned pid $($taskProcess.Id) terminated)"
    }
    Get-Content $taskOut,$taskErr | Add-Content "$taskBuild/logging_build.log"
    if($taskProcess.ExitCode -ne 0){throw "Native stage failed: $taskLabel exit=$($taskProcess.ExitCode)"}
}
Set-Content "$taskBuild/logging_build.log" 'Bounded native stages: 60 seconds each.'
InvokeTaskNative $taskCompiler ($taskFlags+@("$PSScriptRoot/logging_adapter.cpp","/Fo$taskBuild/logging_adapter.obj")) 'adapter'
InvokeTaskNative $taskCompiler ($taskFlags+@("$PSScriptRoot/logging_control.cpp","/Fo$taskBuild/logging_control.obj")) 'control'
InvokeTaskNative $taskCompiler ($taskFlags+@("$taskUp/externals/spdlog/src/sinks/base_sink.cpp","/Fo$taskBuild/logging_base_sink.obj")) 'base_sink'
$taskSupport=Get-Content "$taskRoot/reports/reconstruction/data/shader_support_compile.json" -Raw | ConvertFrom-Json
$taskObjects=@("$taskBuild/logging_adapter.obj","$taskBuild/logging_control.obj","$taskBuild/logging_base_sink.obj")
function TaskHash($taskPath){([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($taskPath)))).Replace('-','').ToLower()}
foreach($taskItem in $taskSupport.files){
    if($taskItem.source.StartsWith('externals/spdlog/') -or $taskItem.source -eq 'externals/fmt/src/format.cc'){
        $taskObject="$taskBuild/$($taskItem.source.Replace('/','_').Replace('.cpp','').Replace('.cc','')).obj"
        if((TaskHash $taskObject) -ne $taskItem.object_sha256){throw 'Stale logging support object'}
        $taskObjects+=$taskObject
    }
}
InvokeTaskNative (Get-Command link.exe).Source (@('/NOLOGO',"/OUT:$taskBuild/logging_control.exe")+$taskObjects+@('kernel32.lib','user32.lib','advapi32.lib','libucrt.lib','libvcruntime.lib')) 'link'
InvokeTaskNative "$taskBuild/logging_control.exe" @() 'run'
Copy-Item "$taskBuild/logging_run.stderr.log" "$taskBuild/logging_control.log"
$taskText=[IO.File]::ReadAllText("$taskBuild/logging_control.log")
$taskRecords=[regex]::Matches($taskText,'(?m)^record worker=([0-3]) row=([0-9]+)\r?$')
$taskUnique=@($taskRecords | ForEach-Object {$_.Value.Trim()} | Select-Object -Unique)
if($taskRecords.Count -ne 200 -or $taskUnique.Count -ne 200 -or -not $taskText.Contains(('payload value=42 utf8='+[char]0x03b1))){throw 'Lost or interleaved log records'}
foreach($taskRecord in $taskRecords){if([int]$taskRecord.Groups[2].Value -ge 50){throw 'Out-of-range log row'}}
$taskFiles=@('engine/shadPS4/externals/spdlog/src/sinks/base_sink.cpp','build/CTRPort-data/logging_base_sink.obj','ctrport/video/logging_adapter.cpp','ctrport/video/logging_control.cpp','ctrport/video/build_logging.ps1','engine/shadPS4/src/common/logging/classes.h','build/CTRPort-data/logging_classes.inc','build/CTRPort-data/logging_adapter.obj','build/CTRPort-data/logging_control.exe','build/CTRPort-data/logging_build.log','build/CTRPort-data/logging_control.log')
$taskFiles+=@('engine/shadPS4/externals/spdlog/LICENSE','reports/reconstruction/data/shader_support_compile.json')
foreach($taskItem in $taskSupport.files){if($taskItem.source.StartsWith('externals/spdlog/') -or $taskItem.source -eq 'externals/fmt/src/format.cc'){$taskFiles+=('engine/shadPS4/'+$taskItem.source);$taskFiles+=('build/CTRPort-data/'+$taskItem.source.Replace('/','_').Replace('.cpp','').Replace('.cc','')+'.obj')}}
$taskHashes=@{};foreach($taskFile in $taskFiles){$taskHashes[$taskFile]=TaskHash "$taskRoot/$taskFile"}
@{checked_at=[DateTime]::UtcNow.ToString('o');passed=$true;upstream_base_sink_license='MIT (engine/shadPS4/externals/spdlog/LICENSE)';native_stage_timeout_seconds=60;classes=$taskClassMatches.Count;records=$taskRecords.Count;unique_records=$taskUnique.Count;hashes=$taskHashes;scope='Host shader stderr logging only; map frozen before workers, no guest service';R3=$false;retail_shaders_tested=0} | ConvertTo-Json -Depth 5 | Set-Content "$taskRoot/reports/reconstruction/data/shader_logging_control.json" -Encoding utf8
Write-Output "Logging control classes=$($taskClassMatches.Count) records=$($taskRecords.Count) unique=$($taskUnique.Count)"
