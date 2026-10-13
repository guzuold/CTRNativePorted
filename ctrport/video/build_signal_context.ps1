# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
if($env:CTR_BUILD_LOCK_HELD -ne '1'){throw 'build-data lock required'}
$taskRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskBuild=Join-Path $taskRoot 'build/CTRPort-data'
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
function InvokeTaskNative($taskExe, $taskArgs, $taskLabel) {
    $taskOut="$taskBuild/signal_context_$taskLabel.stdout.log"
    $taskErr="$taskBuild/signal_context_$taskLabel.stderr.log"
    $taskQuoted=@($taskArgs | ForEach-Object {'"'+$_+'"'})
    $taskStart=@{FilePath=$taskExe;PassThru=$true;WindowStyle='Hidden';RedirectStandardOutput=$taskOut;RedirectStandardError=$taskErr}
    if($taskQuoted.Count -gt 0){$taskStart.ArgumentList=$taskQuoted}
    $taskProcess=Start-Process @taskStart
    $taskHandle=$taskProcess.Handle
    if(-not $taskProcess.WaitForExit(60000)) {
        $taskProcess.Kill(); $taskProcess.WaitForExit()
        throw "Native stage timed out: $taskLabel (owned pid $($taskProcess.Id) terminated)"
    }
    Get-Content $taskOut,$taskErr | Add-Content "$taskBuild/signal_context_build.log"
    if($taskProcess.ExitCode -ne 0){throw "Native stage failed: $taskLabel exit=$($taskProcess.ExitCode)"}
}
Set-Content "$taskBuild/signal_context_build.log" 'Bounded settings control'
$taskCompiler=(Get-Command cl.exe).Source
InvokeTaskNative $taskCompiler @('/nologo','/c','/O2','/W4','/WX','/EHsc','/std:c++17',"$PSScriptRoot/signal_context_adapter.cpp","/Fo$taskBuild/signal_context.obj") 'adapter'
InvokeTaskNative $taskCompiler @('/nologo','/O2','/W4','/WX','/EHsc','/std:c++17',"$PSScriptRoot/signal_context_control.cpp","$taskBuild/signal_context.obj","/Fo$taskBuild/signal_context_control.obj","/Fe$taskBuild/signal_context_control.exe") 'link'
InvokeTaskNative "$taskBuild/signal_context_control.exe" @() 'run'
function TaskHash($taskPath){([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($taskPath)))).Replace('-','').ToLower()}
$taskFiles=@('ctrport/video/signal_context_adapter.cpp','ctrport/video/signal_context_control.cpp','ctrport/video/build_signal_context.ps1','build/CTRPort-data/signal_context.obj','build/CTRPort-data/signal_context_control.exe','build/CTRPort-data/signal_context_build.log')
$taskHashes=@{};foreach($taskFile in $taskFiles){$taskHashes[$taskFile]=TaskHash "$taskRoot/$taskFile"}
@{checked_at=[DateTime]::UtcNow.ToString('o');passed=$true;cases=@('null-rejected','null-context-record-rejected','RtlCaptureContext','full64bit','zero');hashes=$taskHashes;scope='Windows EXCEPTION_POINTERS accessor only. No signal dispatcher installed, no SRT recovery or guest exception support.';R3=$false;retail_shaders_tested=0} | ConvertTo-Json -Depth 5 | Set-Content "$taskRoot/reports/reconstruction/data/signal_context_control.json" -Encoding utf8
Write-Output 'Windows EXCEPTION_POINTERS accessor 5 controls passed'
