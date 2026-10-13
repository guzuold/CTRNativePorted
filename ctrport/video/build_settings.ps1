# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
if($env:CTR_BUILD_LOCK_HELD -ne '1'){throw 'build-data lock required'}
$taskRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskBuild=Join-Path $taskRoot 'build/CTRPort-data'
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
function InvokeTaskNative($taskExe, $taskArgs, $taskLabel) {
    $taskOut="$taskBuild/settings_$taskLabel.stdout.log"
    $taskErr="$taskBuild/settings_$taskLabel.stderr.log"
    $taskQuoted=@($taskArgs | ForEach-Object {'"'+$_+'"'})
    $taskStart=@{FilePath=$taskExe;PassThru=$true;WindowStyle='Hidden';RedirectStandardOutput=$taskOut;RedirectStandardError=$taskErr}
    if($taskQuoted.Count -gt 0){$taskStart.ArgumentList=$taskQuoted}
    $taskProcess=Start-Process @taskStart
    $taskHandle=$taskProcess.Handle
    if(-not $taskProcess.WaitForExit(60000)) {
        $taskProcess.Kill(); $taskProcess.WaitForExit()
        throw "Native stage timed out: $taskLabel (owned pid $($taskProcess.Id) terminated)"
    }
    Get-Content $taskOut,$taskErr | Add-Content "$taskBuild/settings_build.log"
    if($taskProcess.ExitCode -ne 0){throw "Native stage failed: $taskLabel exit=$($taskProcess.ExitCode)"}
}
Set-Content "$taskBuild/settings_build.log" 'Bounded settings control'
$taskCompiler=(Get-Command cl.exe).Source
InvokeTaskNative $taskCompiler @('/nologo','/c','/O2','/W4','/WX','/EHsc','/std:c++17',"$PSScriptRoot/shader_settings.cpp","/Fo$taskBuild/shader_settings.obj") 'adapter'
InvokeTaskNative $taskCompiler @('/nologo','/O2','/W4','/WX','/EHsc','/std:c++17',"$PSScriptRoot/shader_settings_control.cpp","$taskBuild/shader_settings.obj","/Fo$taskBuild/shader_settings_control.obj","/Fe$taskBuild/shader_settings_control.exe") 'link'
foreach($taskDump in @('0','1')){foreach($taskDirect in @('0','1')){InvokeTaskNative "$taskBuild/shader_settings_control.exe" @($taskDump,$taskDirect) "run_$taskDump$taskDirect"}}
function TaskHash($taskPath){([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($taskPath)))).Replace('-','').ToLower()}
$taskFiles=@('ctrport/video/shader_settings.h','ctrport/video/shader_settings.cpp','ctrport/video/shader_settings_control.cpp','ctrport/video/build_settings.ps1','build/CTRPort-data/shader_settings.obj','build/CTRPort-data/shader_settings_control.exe','build/CTRPort-data/settings_build.log')
$taskHashes=@{};foreach($taskFile in $taskFiles){$taskHashes[$taskFile]=TaskHash "$taskRoot/$taskFile"}
@{checked_at=[DateTime]::UtcNow.ToString('o');passed=$true;cases=4;reads_per_case=4000;preinitialization_rejected=$true;reinitialization_rejected=$true;hashes=$taskHashes;scope='Explicit immutable host flags only. Not yet substituted into upstream shader compilation. True flags do not implement file dump or direct memory services.';R3=$false;retail_shaders_tested=0} | ConvertTo-Json -Depth 5 | Set-Content "$taskRoot/reports/reconstruction/data/shader_settings_control.json" -Encoding utf8
Write-Output 'Settings control 4 flag combinations, 16000 concurrent reads passed'
