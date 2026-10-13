# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
if($env:CTR_BUILD_LOCK_HELD -ne '1'){throw 'build-data lock required'}
$taskRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskBuild=Join-Path $taskRoot 'build/CTRPort-data'
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
function InvokeTaskNative($taskExe, $taskArgs, $taskLabel) {
    $taskOut="$taskBuild/shader_directory_pin_$taskLabel.stdout.log"
    $taskErr="$taskBuild/shader_directory_pin_$taskLabel.stderr.log"
    $taskQuoted=@($taskArgs | ForEach-Object {'"'+$_+'"'})
    $taskStart=@{FilePath=$taskExe;PassThru=$true;WindowStyle='Hidden';RedirectStandardOutput=$taskOut;RedirectStandardError=$taskErr}
    if($taskQuoted.Count -gt 0){$taskStart.ArgumentList=$taskQuoted}
    $taskProcess=Start-Process @taskStart
    $taskHandle=$taskProcess.Handle
    if(-not $taskProcess.WaitForExit(60000)) {
        $taskProcess.Kill(); $taskProcess.WaitForExit()
        throw "Native stage timed out: $taskLabel (owned pid $($taskProcess.Id) terminated)"
    }
    Get-Content $taskOut,$taskErr | Add-Content "$taskBuild/shader_directory_pin_build.log"
    if($taskProcess.ExitCode -ne 0){throw "Native stage failed: $taskLabel exit=$($taskProcess.ExitCode)"}
}
Set-Content "$taskBuild/shader_directory_pin_build.log" 'Bounded directory pin control'
$taskCompiler=(Get-Command cl.exe).Source
InvokeTaskNative $taskCompiler @('/nologo','/c','/O2','/W4','/WX','/EHsc','/std:c++17',"$PSScriptRoot/shader_directory_pin.cpp","/Fo$taskBuild/shader_directory_pin.obj") 'adapter'
InvokeTaskNative $taskCompiler @('/nologo','/c','/O2','/W4','/WX','/EHsc','/std:c++17',"$PSScriptRoot/shader_path.cpp","/Fo$taskBuild/shader_path.obj") 'path'
InvokeTaskNative $taskCompiler @('/nologo','/O2','/W4','/WX','/EHsc','/std:c++17',"$PSScriptRoot/shader_directory_pin_control.cpp","$taskBuild/shader_path.obj","$taskBuild/shader_directory_pin.obj","/Fo$taskBuild/shader_directory_pin_control.obj","/Fe$taskBuild/shader_directory_pin_control.exe") 'link'
InvokeTaskNative "$taskBuild/shader_directory_pin_control.exe" @($taskRoot) 'run'
$taskGlobal=Join-Path $taskRoot ('runtime/user/shader/data-global-session-'+[Guid]::NewGuid().ToString('N'))
$taskGlobalDumps=Join-Path $taskGlobal 'runtime/user/shader/dumps'
New-Item -ItemType Directory -Path $taskGlobalDumps -Force | Out-Null
try {
    InvokeTaskNative "$taskBuild/shader_directory_pin_control.exe" @($taskRoot,$taskGlobal,'global') 'global'
} finally {
    # Exact owned paths only, after the bounded child exits; no recursive removal.
    foreach($taskSuffix in @('runtime/user/shader/dumps','runtime/user/shader','runtime/user','runtime','')) {
        $taskDirectory=if($taskSuffix){Join-Path $taskGlobal $taskSuffix}else{$taskGlobal}
        [IO.Directory]::Delete($taskDirectory,$false)
    }
}
$taskShader=Join-Path $taskRoot 'runtime/user/shader'
$taskJunction=Join-Path $taskShader ('data-pin-junction-'+[Guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Junction -Path $taskJunction -Target (Join-Path $taskRoot 'runtime/user') | Out-Null
    InvokeTaskNative "$taskBuild/shader_directory_pin_control.exe" @($taskRoot,$taskJunction) 'junction'
    InvokeTaskNative "$taskBuild/shader_directory_pin_control.exe" @($taskRoot,(Join-Path $taskJunction 'shader')) 'junction_ancestor'
} finally {
    if(Test-Path -LiteralPath $taskJunction) {
        $taskItem=Get-Item -LiteralPath $taskJunction -Force
        if($taskItem.LinkType -ne 'Junction' -or $taskItem.Parent.FullName -ne $taskShader){throw 'Unexpected junction cleanup target'}
        [IO.Directory]::Delete($taskJunction,$false)
    }
}
function TaskHash($taskPath){([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($taskPath)))).Replace('-','').ToLower()}
$taskFiles=@('ctrport/video/shader_path.h','ctrport/video/shader_path.cpp','ctrport/video/shader_directory_pin.h','ctrport/video/shader_directory_pin.cpp','ctrport/video/shader_directory_pin_control.cpp','ctrport/video/build_shader_directory_pin.ps1','build/CTRPort-data/shader_directory_pin.obj','build/CTRPort-data/shader_path.obj','build/CTRPort-data/shader_directory_pin_control.exe','build/CTRPort-data/shader_directory_pin_build.log')
$taskHashes=@{};foreach($taskFile in $taskFiles){$taskHashes[$taskFile]=TaskHash "$taskRoot/$taskFile"}
@{checked_at=[DateTime]::UtcNow.ToString('o');passed=$true;cases=@('rename-blocked-while-pinned','delete-blocked-while-pinned','rename-after-release','partial-acquisition-cleanup','existing-delete-access-rejected','opened-junction-leaf-rejected','opened-junction-ancestor-rejected','session-preinit-rejected','local-session-output','session-nonleaf-rejected','stream-two-writes-finish-readback','stream-write-after-finish-rejected','existing-leaf-rejected','invalid-extent-before-IO','existing-hardlink-rejected-target-size-unchanged','empty-output','missing-parent-explicit-no-directory-created','global-publication-16000-reads-8-threads','global-reinit-rejected-identity-preserved','global-stream-readback');hashes=$taskHashes;scope='Existing local directory chain only, disposable runtime/user/shader fixture removed. Exclusive new output with readback tested; existing leaves rejected. Global process-lifetime publication identity tested across16000reads/8threads, reinit rejected and stream readback passed; fixture directories removed after child exit. Concurrent initialization race, write/flush failure injection and retail shaders not tested. Opened-handle reparse rejection tested at leaf and ancestor using a temporary junction targeting runtime/user, removed nonrecursively.';R3=$false} | ConvertTo-Json -Depth 5 | Set-Content "$taskRoot/reports/reconstruction/data/shader_directory_pin_control.json" -Encoding utf8
