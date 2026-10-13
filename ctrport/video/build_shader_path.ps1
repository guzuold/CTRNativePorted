# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
if($env:CTR_BUILD_LOCK_HELD -ne '1'){throw 'build-data lock required'}
$taskRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskBuild=Join-Path $taskRoot 'build/CTRPort-data'
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
function InvokeTaskNative($taskExe, $taskArgs, $taskLabel) {
    $taskOut="$taskBuild/shader_path_$taskLabel.stdout.log"
    $taskErr="$taskBuild/shader_path_$taskLabel.stderr.log"
    $taskQuoted=@($taskArgs | ForEach-Object {'"'+$_+'"'})
    $taskStart=@{FilePath=$taskExe;PassThru=$true;WindowStyle='Hidden';RedirectStandardOutput=$taskOut;RedirectStandardError=$taskErr}
    if($taskQuoted.Count -gt 0){$taskStart.ArgumentList=$taskQuoted}
    $taskProcess=Start-Process @taskStart
    $taskHandle=$taskProcess.Handle
    if(-not $taskProcess.WaitForExit(60000)) {
        $taskProcess.Kill(); $taskProcess.WaitForExit()
        throw "Native stage timed out: $taskLabel (owned pid $($taskProcess.Id) terminated)"
    }
    Get-Content $taskOut,$taskErr | Add-Content "$taskBuild/shader_path_build.log"
    if($taskProcess.ExitCode -ne 0){throw "Native stage failed: $taskLabel exit=$($taskProcess.ExitCode)"}
}
Set-Content "$taskBuild/shader_path_build.log" 'Bounded settings control'
$taskCompiler=(Get-Command cl.exe).Source
InvokeTaskNative $taskCompiler @('/nologo','/c','/O2','/W4','/WX','/EHsc','/std:c++17',"$PSScriptRoot/shader_path.cpp","/Fo$taskBuild/shader_path.obj") 'adapter'
InvokeTaskNative $taskCompiler @('/nologo','/O2','/W4','/WX','/EHsc','/std:c++17',"$PSScriptRoot/shader_path_control.cpp","$taskBuild/shader_path.obj","/Fo$taskBuild/shader_path_control.obj","/Fe$taskBuild/shader_path_control.exe") 'link'
InvokeTaskNative "$taskBuild/shader_path_control.exe" @($taskRoot) 'run'
$taskUser=Join-Path $taskRoot 'runtime/user'
$taskShader=Join-Path $taskUser 'shader'
New-Item -ItemType Directory -Force -Path $taskShader | Out-Null
$taskLeaf='data-path-control-'+[Guid]::NewGuid().ToString('N')
$taskJunction=Join-Path $taskShader $taskLeaf
try {
    New-Item -ItemType Junction -Path $taskJunction -Target $taskUser | Out-Null
    InvokeTaskNative "$taskBuild/shader_path_control.exe" @($taskRoot,"$taskLeaf/rejected.bin") 'junction'
} finally {
    if(Test-Path -LiteralPath $taskJunction){
        $taskItem=Get-Item -LiteralPath $taskJunction -Force
        if($taskItem.LinkType -ne 'Junction' -or $taskItem.Parent.FullName -ne $taskShader){throw 'Refusing unexpected cleanup target'}
        [IO.Directory]::Delete($taskJunction,$false)
    }
}
function TaskHash($taskPath){([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($taskPath)))).Replace('-','').ToLower()}
$taskFiles=@('ctrport/video/shader_path.h','ctrport/video/shader_path.cpp','ctrport/video/shader_path_control.cpp','ctrport/video/build_shader_path.ps1','build/CTRPort-data/shader_path.obj','build/CTRPort-data/shader_path_control.exe','build/CTRPort-data/shader_path_build.log')
$taskHashes=@{};foreach($taskFile in $taskFiles){$taskHashes[$taskFile]=TaskHash "$taskRoot/$taskFile"}
@{checked_at=[DateTime]::UtcNow.ToString('o');passed=$true;cases=@('valid-nested','eighteen-escape-or-device-alias-rejections','real-junction-rejected-and-removed','relative-root-rejected');hashes=$taskHashes;scope='Validation only. Control creates then removes a unique junction under runtime/user/shader targeting runtime/user. No output file opened. Concurrent path replacement race not solved; require owned directory handles before actual IO integration.';R3=$false;retail_shaders_tested=0} | ConvertTo-Json -Depth 5 | Set-Content "$taskRoot/reports/reconstruction/data/shader_path_control.json" -Encoding utf8
Write-Output 'Shader path validation and real junction rejection passed'
