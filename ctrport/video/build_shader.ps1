# SPDX-License-Identifier: GPL-2.0-only
# Compile diagnostic only. Upstream source read in place, never modified.
param([switch]$All, [switch]$ContinueOnError, [switch]$Support, [switch]$LinkDiagnostic, [switch]$Settings, [switch]$Dump, [switch]$SliceControl, [switch]$Bounded, [switch]$CompilerControl, [switch]$CompilerCompanion)
if($CompilerControl -or $CompilerCompanion){$LinkDiagnostic=$true}
$ErrorActionPreference='Stop'
if($env:CTR_BUILD_LOCK_HELD -ne '1'){throw 'build-data lock required'}
$taskRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskUp=Join-Path $taskRoot 'engine/shadPS4'
$taskBuild=Join-Path $taskRoot 'build/CTRPort-data'
New-Item -ItemType Directory -Force $taskBuild | Out-Null
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$taskCandidates=@($env:CTR_CLANG_CL, (Join-Path $taskRoot 'tools/toolchains/llvm-19.1.1/bin/clang-cl.exe'), 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-cl.exe', 'C:/Program Files/LLVM/bin/clang-cl.exe')
$taskCompiler=$taskCandidates | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1
if(-not $taskCompiler){
    'MISSING clang-cl: upstream shadPS4 requires Clang SysV ABI support; no source adaptation or installation attempted.' | Set-Content "$taskBuild/shader_compiler_missing.log"
    Get-Content "$taskBuild/shader_compiler_missing.log"
    exit 2
}
$ErrorActionPreference='Continue' # Native warnings on stderr are not compiler failure.
if($SliceControl){
    $ErrorActionPreference='Continue'
    $taskSliceFlags=@('/utf-8','/nologo','/c','/O2','/W4','/EHsc','/std:c++latest','/DNOMINMAX',"/I$PSScriptRoot/adapted/include","/I$taskUp/src","/I$taskUp/externals/fmt/include","/I$taskUp/externals/spdlog/include","/I$taskUp/externals/magic_enum/include")
    $taskSliceObjects=@()
    foreach($taskUnit in @(@('decode',"$taskUp/src/shader_recompiler/frontend/decode.cpp"),@('format',"$taskUp/src/shader_recompiler/frontend/format.cpp"),@('control',"$PSScriptRoot/shader_slice_control.cpp"))){
        $taskObject="$taskBuild/shader_slice_$($taskUnit[0]).obj"
        & $taskCompiler @taskSliceFlags $taskUnit[1] "/Fo$taskObject" *> "$taskBuild/shader_slice_$($taskUnit[0]).log"
        if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/shader_slice_$($taskUnit[0]).log" -Tail 12;exit $LASTEXITCODE}
        $taskSliceObjects+=$taskObject
    }
    $ErrorActionPreference='Stop'
    foreach($taskReportName in @('shader_fatal_control.json','shader_support_compile.json')){
        $taskReport=Get-Content "$taskRoot/reports/reconstruction/data/$taskReportName" -Raw | ConvertFrom-Json
        if($taskReportName -eq 'shader_fatal_control.json'){$taskExpected=$taskReport.hashes.'build/CTRPort-data/fatal_adapter.obj';$taskDependency="$taskBuild/fatal_adapter.obj"}
        else{$taskItem=$taskReport.files | Where-Object {$_.source -eq 'externals/fmt/src/format.cc'};$taskExpected=$taskItem.object_sha256;$taskDependency="$taskBuild/externals_fmt_src_format.obj"}
        $taskHash=([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($taskDependency)))).Replace('-','').ToLower()
        if($taskHash -ne $taskExpected){throw 'Stale slice dependency'}
        $taskSliceObjects+=$taskDependency
    }
    $taskSupport=Get-Content "$taskRoot/reports/reconstruction/data/shader_support_compile.json" -Raw | ConvertFrom-Json
    foreach($taskItem in @($taskSupport.files | Where-Object {$_.source.StartsWith('externals/spdlog/')})){
        $taskDependency="$taskBuild/"+$taskItem.source.Replace('/','_').Replace('.cpp','')+'.obj'
        $taskHash=([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($taskDependency)))).Replace('-','').ToLower()
        if($taskHash -ne $taskItem.object_sha256 -or $taskItem.exit_code -ne 0){throw 'Stale slice logging support'}
        $taskSliceObjects+=$taskDependency
    }
    foreach($taskControlName in @('shader_host_control.json','shader_logging_control.json')){
        $taskControl=Get-Content "$taskRoot/reports/reconstruction/data/$taskControlName" -Raw | ConvertFrom-Json
        if(-not $taskControl.passed){throw 'Host/logging control required'}
        foreach($taskProperty in $taskControl.hashes.PSObject.Properties){
            $taskHash=([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes("$taskRoot/$($taskProperty.Name)")))).Replace('-','').ToLower()
            if($taskHash -ne $taskProperty.Value){throw 'Stale host/logging control'}
        }
    }
    $taskSliceObjects+=@("$taskBuild/thread_name_adapter.obj","$taskBuild/logging_adapter.obj","$taskBuild/logging_base_sink.obj")
    $ErrorActionPreference='Continue'
    & link.exe /NOLOGO /OUT:"$taskBuild/shader_slice_control.exe" @taskSliceObjects kernel32.lib libucrt.lib libvcruntime.lib *> "$taskBuild/shader_slice_control_build.log"
    if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/shader_slice_control_build.log" -Tail 12;exit $LASTEXITCODE}
    & "$taskBuild/shader_slice_control.exe" *> "$taskBuild/shader_slice_control_run.log"
    exit $LASTEXITCODE
}
if($LinkDiagnostic){
    $ErrorActionPreference='Stop'
    $taskObjects=@()
    foreach($taskReportName in @('shader_full_compile_diagnostic.json','shader_support_compile.json')){
        $taskReport=Get-Content "$taskRoot/reports/reconstruction/data/$taskReportName" -Raw | ConvertFrom-Json
        foreach($taskItem in $taskReport.files){
            if($taskItem.exit_code -ne 0){throw 'Complete compilation required'}
            $taskSourcePath="$taskUp/$($taskItem.source)"
            $taskSourceHash=([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($taskSourcePath)))).Replace('-','').ToLower()
            if($taskSourceHash -ne $taskItem.source_sha256){throw "Stale compiled source $($taskItem.source)"}
            $taskName=$taskItem.source.Replace('/','_').Replace('.cpp','').Replace('.cc','')
            $taskObject="$taskBuild/$taskName.obj"
            if(([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($taskObject)))).Replace('-','').ToLower() -ne $taskItem.object_sha256){throw "Stale object $taskName"}
            $taskObjects+= $taskObject
        }
    }
    $taskSettings=Get-Content "$taskRoot/reports/reconstruction/data/shader_settings_compile.json" -Raw | ConvertFrom-Json
    $taskSettingsControl=Get-Content "$taskRoot/reports/reconstruction/data/shader_settings_control.json" -Raw | ConvertFrom-Json
    if($taskSettings.passed -ne 5 -or -not $taskSettingsControl.passed){throw 'Settings control and compilation required'}
    foreach($taskMap in @($taskSettings.shim_hashes,$taskSettingsControl.hashes)){
        foreach($taskProperty in $taskMap.PSObject.Properties){
            $taskHash=([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes("$taskRoot/$($taskProperty.Name)")))).Replace('-','').ToLower()
            if($taskHash -ne $taskProperty.Value){throw "Stale settings evidence $($taskProperty.Name)"}
        }
    }
    foreach($taskItem in $taskSettings.files){
        $taskName=$taskItem.source.Replace('/','_').Replace('.cpp','')
        $taskObject="$taskBuild/$taskName.settings.obj"
        foreach($taskPair in @(@("$taskUp/$($taskItem.source)",$taskItem.source_sha256),@($taskObject,$taskItem.object_sha256))){
            $taskHash=([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($taskPair[0])))).Replace('-','').ToLower()
            if($taskHash -ne $taskPair[1]){throw 'Stale settings source/object'}
        }
        $taskObjects=@($taskObjects | Where-Object {$_ -ne "$taskBuild/$taskName.obj"})
        $taskObjects+=$taskObject
    }
    $taskObjects+="$taskBuild/shader_settings.obj"
    $taskSignal=Get-Content "$taskRoot/reports/reconstruction/data/signal_context_control.json" -Raw | ConvertFrom-Json
    if(-not $taskSignal.passed){throw 'Signal context control required'}
    foreach($taskProperty in $taskSignal.hashes.PSObject.Properties){
        $taskHash=([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes("$taskRoot/$($taskProperty.Name)")))).Replace('-','').ToLower()
        if($taskHash -ne $taskProperty.Value){throw 'Stale signal context evidence'}
    }
    $taskObjects+="$taskBuild/signal_context.obj"
    $taskFatal=Get-Content "$taskRoot/reports/reconstruction/data/shader_fatal_control.json" -Raw | ConvertFrom-Json
    $taskFatalObject="$taskBuild/fatal_adapter.obj"
    $taskFatalHash=([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($taskFatalObject)))).Replace('-','').ToLower()
    if($taskFatalHash -ne $taskFatal.hashes.'build/CTRPort-data/fatal_adapter.obj'){throw 'Stale fatal adapter object'}
    if(@($taskFatal.cases | Where-Object {$_.exit_code -eq 86 -and -not $_.returned}).Count -ne 2){throw 'Fatal policy control required'}
    $taskObjects+=$taskFatalObject
    $taskHost=Get-Content "$taskRoot/reports/reconstruction/data/shader_host_control.json" -Raw | ConvertFrom-Json
    if(-not $taskHost.passed){throw 'Host control required'}
    $taskHostObject="$taskBuild/thread_name_adapter.obj"
    $taskHostHash=([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($taskHostObject)))).Replace('-','').ToLower()
    if($taskHostHash -ne $taskHost.hashes.'build/CTRPort-data/thread_name_adapter.obj'){throw 'Stale host adapter object'}
    $taskObjects+=$taskHostObject
    $taskLogging=Get-Content "$taskRoot/reports/reconstruction/data/shader_logging_control.json" -Raw | ConvertFrom-Json
    if(-not $taskLogging.passed -or $taskLogging.records -ne 200 -or $taskLogging.unique_records -ne 200){throw 'Logging control required'}
    foreach($taskProperty in $taskLogging.hashes.PSObject.Properties){
        $taskCurrentHash=([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes("$taskRoot/$($taskProperty.Name)")))).Replace('-','').ToLower()
        if($taskCurrentHash -ne $taskProperty.Value){throw "Stale logging evidence $($taskProperty.Name)"}
    }
    $taskObjects+=@("$taskBuild/logging_adapter.obj","$taskBuild/logging_base_sink.obj")
    foreach($taskControl in @($taskFatal,$taskHost)){
        foreach($taskProperty in $taskControl.hashes.PSObject.Properties){
            if($taskProperty.Name.StartsWith('ctrport/video/')){
                $taskCurrentHash=([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes("$taskRoot/$($taskProperty.Name)")))).Replace('-','').ToLower()
                if($taskCurrentHash -ne $taskProperty.Value){throw "Stale host control source $($taskProperty.Name)"}
            }
        }
    }
    function TaskLinkHash($path){([BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([IO.File]::ReadAllBytes($path)))).Replace('-','').ToLower()}
    $taskDump=Get-Content "$taskRoot/reports/reconstruction/data/shader_dump_compile.json" -Raw | ConvertFrom-Json
    $taskDumpManifest=Get-Content "$PSScriptRoot/adapted/manifest.json" -Raw | ConvertFrom-Json
    if(-not $taskDump.passed -or @($taskDump.compile_units).Count -ne 3 -or (TaskLinkHash "$PSScriptRoot/adapted/manifest.json") -ne $taskDump.manifest_sha256){throw 'Dump compile evidence missing or stale'}
    foreach($taskItem in $taskDumpManifest){
        if((TaskLinkHash "$taskUp/$($taskItem.source)") -ne $taskItem.source_sha256 -or (TaskLinkHash "$taskRoot/$($taskItem.adapted)") -ne $taskItem.adapted_sha256){throw 'Stale dump source'}
        $taskUnit=@($taskDump.compile_units | Where-Object {$_.source -eq $taskItem.source})
        $taskName=$taskItem.source.Replace('/','_').Replace('.cpp','')
        $taskObject="$taskBuild/$taskName.dump.obj"
        if($taskUnit.Count -ne 1 -or $taskUnit[0].exit_code -ne 0 -or (TaskLinkHash $taskObject) -ne $taskUnit[0].object_sha256){throw 'Stale dump object'}
        $taskObjects=@($taskObjects | Where-Object {$_ -ne "$taskBuild/$taskName.settings.obj"})
        $taskObjects+=$taskObject
    }
    $taskPin=Get-Content "$taskRoot/reports/reconstruction/data/shader_directory_pin_control.json" -Raw | ConvertFrom-Json
    if(-not $taskPin.passed -or -not $taskPin.hashes.'build/CTRPort-data/shader_path.obj'){throw 'Pin/path control required'}
    foreach($taskProperty in $taskPin.hashes.PSObject.Properties){
        if((TaskLinkHash "$taskRoot/$($taskProperty.Name)") -ne $taskProperty.Value){throw "Stale pin evidence $($taskProperty.Name)"}
    }
    $taskObjects+=@("$taskBuild/shader_directory_pin.obj","$taskBuild/shader_path.obj")
    $taskBounded=Get-Content "$taskRoot/reports/reconstruction/data/shader_bounded_compile.json" -Raw | ConvertFrom-Json
    $taskBoundedManifest=Get-Content "$PSScriptRoot/adapted/bounded_manifest.json" -Raw | ConvertFrom-Json
    if(-not $taskBounded.passed -or @($taskBounded.files).Count -ne 6 -or (TaskLinkHash "$PSScriptRoot/adapted/bounded_manifest.json") -ne $taskBounded.manifest_sha256 -or (TaskLinkHash "$taskRoot/$($taskBoundedManifest.header)") -ne $taskBounded.header_sha256){throw 'Stale bounded compile evidence'}
    foreach($taskUnit in $taskBoundedManifest.units){
        $taskItem=@($taskBounded.files | Where-Object {$_.source -eq $taskUnit.source})
        $taskName=$taskUnit.source.Replace('/','_').Replace('.cpp','')
        $taskObject="$taskBuild/$taskName.bounded.obj"
        if($taskItem.Count -ne 1 -or $taskItem[0].exit_code -ne 0 -or (TaskLinkHash $taskObject) -ne $taskItem[0].object_sha256 -or (TaskLinkHash "$taskRoot/$($taskUnit.compile_source)") -ne $taskUnit.compile_source_sha256 -or (TaskLinkHash "$taskUp/$($taskUnit.source)") -ne $taskUnit.source_sha256){throw 'Stale bounded source/object'}
        $taskObjects=@($taskObjects | Where-Object {$_ -ne "$taskBuild/$taskName.obj" -and $_ -ne "$taskBuild/$taskName.settings.obj" -and $_ -ne "$taskBuild/$taskName.dump.obj"})
        $taskObjects+=$taskObject
    }

    $ErrorActionPreference='Continue'
    # Whole pinned resource pass with optional sanitized provenance observer.
    $taskTraceObject="$taskBuild/resource_tracking_provenance.obj"
    $taskTraceBoost=@(Get-ChildItem "$taskUp/externals/ext-boost/libs" -Directory | ForEach-Object { Join-Path $_.FullName 'include' } | Where-Object {Test-Path -LiteralPath $_} | ForEach-Object {"/I$_"})
    & $taskCompiler /utf-8 /nologo /c /O2 /W4 /EHsc /std:c++latest /DNOMINMAX @taskTraceBoost "/I$PSScriptRoot" "/I$PSScriptRoot/adapted/include" "/I$PSScriptRoot/shim" "/I$taskUp/src" "/I$taskUp/externals/ext-boost" "/I$taskUp/externals/fmt/include" "/I$taskUp/externals/spdlog/include" "/I$taskUp/externals/magic_enum/include" "$PSScriptRoot/adapted/src_shader_recompiler_ir_passes_resource_tracking_pass.cpp" "/Fo$taskTraceObject" *> "$taskBuild/resource_tracking_provenance_compile.log"
    if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/resource_tracking_provenance_compile.log" -Tail 12;exit $LASTEXITCODE}
    $taskObjects=@($taskObjects | Where-Object {$_ -ne "$taskBuild/src_shader_recompiler_ir_passes_resource_tracking_pass.obj"})
    $taskObjects+=$taskTraceObject
    $ErrorActionPreference='Continue'
    if($CompilerCompanion){
        $taskBoostIncludes=@(Get-ChildItem "$taskUp/externals/ext-boost/libs" -Directory | ForEach-Object { Join-Path $_.FullName 'include' } | Where-Object {Test-Path -LiteralPath $_} | ForEach-Object {"/I$_"})
        & $taskCompiler /utf-8 /nologo /c /O2 /W4 /EHsc /std:c++latest /DNOMINMAX @taskBoostIncludes "/I$PSScriptRoot/adapted/include" "/I$PSScriptRoot/shim" "/I$taskUp/src" "/I$taskUp/externals/ext-boost" "/I$taskUp/externals/fmt/include" "/I$taskUp/externals/spdlog/include" "/I$taskUp/externals/magic_enum/include" "/I$taskUp/externals/vulkan-headers/include" "$PSScriptRoot/graphics_companion.cpp" "/Fo$taskBuild/graphics_companion.obj" *> "$taskBuild/graphics_companion_compile.log"
        if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/graphics_companion_compile.log" -Tail 12;exit $LASTEXITCODE}
        & link.exe /NOLOGO /DLL /OUT:"$taskBuild/ctr_graphics_companion_v34.dll" /IMPLIB:"$taskBuild/ctr_graphics_companion_v34.lib" "$taskBuild/graphics_companion.obj" @taskObjects kernel32.lib user32.lib advapi32.lib libucrt.lib libvcruntime.lib *> "$taskBuild/graphics_companion_link.log"
        if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/graphics_companion_link.log" -Tail 12;exit $LASTEXITCODE}
        & cl.exe /nologo /std:c++17 /EHsc /W4 /WX "$PSScriptRoot/graphics_companion_control.cpp" /Fo"$taskBuild/graphics_companion_control.obj" /Fe"$taskBuild/graphics_companion_control.exe" /link "$taskBuild/ctr_graphics_companion_v34.lib" advapi32.lib *> "$taskBuild/graphics_companion_control_compile.log"
        if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/graphics_companion_control_compile.log" -Tail 12;exit $LASTEXITCODE}
        & "$taskBuild/graphics_companion_control.exe" *> "$taskBuild/graphics_companion_control_run.log"
        $taskCode=$LASTEXITCODE
        @{exit_code=$taskCode;objects=$taskObjects.Count+1;scope='C++17 ABI authored VS/PS diagnostics; no retail profile or shader'} | ConvertTo-Json | Set-Content "$taskBuild/graphics_companion_result.json"
        exit $taskCode
    }
    if($CompilerControl){
        $taskBoostIncludes=@(Get-ChildItem "$taskUp/externals/ext-boost/libs" -Directory | ForEach-Object { Join-Path $_.FullName 'include' } | Where-Object {Test-Path -LiteralPath $_} | ForEach-Object {"/I$_"})
        & $taskCompiler /utf-8 /nologo /c /O2 /W4 /EHsc /std:c++latest /DNOMINMAX @taskBoostIncludes "/I$PSScriptRoot/adapted/include" "/I$PSScriptRoot/shim" "/I$taskUp/src" "/I$taskUp/externals/ext-boost" "/I$taskUp/externals/fmt/include" "/I$taskUp/externals/spdlog/include" "/I$taskUp/externals/magic_enum/include" "/I$taskUp/externals/vulkan-headers/include" "$PSScriptRoot/shader_compiler_control.cpp" "/Fo$taskBuild/shader_compiler_control.obj" *> "$taskBuild/shader_compiler_control_compile.log"
        if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/shader_compiler_control_compile.log" -Tail 12;exit $LASTEXITCODE}
        & link.exe /NOLOGO /OUT:"$taskBuild/shader_compiler_control.exe" "$taskBuild/shader_compiler_control.obj" @taskObjects kernel32.lib user32.lib advapi32.lib libucrt.lib libvcruntime.lib *> "$taskBuild/shader_compiler_control_link.log"
        if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/shader_compiler_control_link.log" -Tail 12;exit $LASTEXITCODE}
        & "$taskBuild/shader_compiler_control.exe" *> "$taskBuild/shader_compiler_control_run.log"
        $taskCode=$LASTEXITCODE
        @{exit_code=$taskCode;objects=$taskObjects.Count+1;scope='Synthetic compiler boundary control, no retail shader'} | ConvertTo-Json | Set-Content "$taskBuild/shader_compiler_control_result.json"
        exit $taskCode
    }
    & link.exe /NOLOGO /DLL /NOENTRY /OPT:NOREF /OUT:"$taskBuild/shader_link_diagnostic.dll" @taskObjects kernel32.lib user32.lib advapi32.lib libucrt.lib libvcruntime.lib *> "$taskBuild/shader_link_diagnostic.log"
    $taskCode=$LASTEXITCODE
    @{exit_code=$taskCode;objects=$taskObjects.Count;purpose='All-object no-entry DLL diagnostic only; no shader entrypoint executed'} | ConvertTo-Json | Set-Content "$taskBuild/shader_link_result.json"
    Get-Content "$taskBuild/shader_link_diagnostic.log" -Tail 4
    exit $taskCode
}
if($Support){
    $taskSources=@(Get-ChildItem "$taskUp/externals/sirit/src" -Filter '*.cpp' -Recurse | Sort-Object FullName | ForEach-Object {$_.FullName.Substring($taskUp.Length+1).Replace('\','/')})
    $taskSources+= @('externals/fmt/src/format.cc','externals/spdlog/src/pattern_formatter.cpp','externals/spdlog/src/details/os_windows.cpp','externals/spdlog/src/common.cpp','externals/spdlog/src/logger.cpp','externals/spdlog/src/details/log_msg.cpp','externals/spdlog/src/details/err_helper.cpp','src/video_core/amdgpu/pixel_format.cpp')
    $taskSources+= @('src/common/decoder.cpp')
    foreach($taskDependency in @('externals/zydis','externals/zydis/dependencies/zycore')){
        $taskCmake=Get-Content "$taskUp/$taskDependency/CMakeLists.txt" -Raw
        $taskSources+= @([regex]::Matches($taskCmake,'"(src/[^"]+\.c)"') | ForEach-Object {"$taskDependency/$($_.Groups[1].Value)"} | Select-Object -Unique)
    }
    $taskResults=@()
    $taskBatchExit=0
    foreach($taskSource in $taskSources){
        $taskName=$taskSource.Replace('/','_').Replace('.cpp','').Replace('.cc','')
        $taskLog="$taskBuild/$taskName.log"
        $taskObject="$taskBuild/$taskName.obj"
        & $taskCompiler /utf-8 /nologo /c /O2 /W4 /EHsc /std:c++latest /DNOMINMAX /DZYDIS_STATIC_BUILD /DZYCORE_STATIC_BUILD "/I$taskUp/externals/zydis/src" "/I$taskUp/externals/zydis/include" "/I$taskUp/externals/zydis/dependencies/zycore/include" "/I$taskUp/externals/sirit/include" "/I$taskUp/externals/sirit/src" "/I$taskUp/src" "/I$taskUp/externals/spdlog/include" "/I$taskUp/externals/sirit/externals/SPIRV-Headers/include" "/I$taskUp/externals/fmt/include" "$taskUp/$taskSource" "/Fo$taskObject" *> $taskLog
        $taskCode=$LASTEXITCODE
        $taskResults+=@{source=$taskSource;exit_code=$taskCode;log=$taskLog;object=$taskObject}
        if($taskCode -ne 0){$taskBatchExit=1}
    }
    $taskResults | ConvertTo-Json -Depth 4 | Set-Content "$taskBuild/shader_support_results.json"
    Write-Output "Shader support attempted=$($taskResults.Count) passed=$(@($taskResults | Where-Object {$_.exit_code -eq 0}).Count); link=false"
    exit $taskBatchExit
}
if($All -or $Settings -or $Dump -or $Bounded){
    $taskBoostIncludes=@(Get-ChildItem "$taskUp/externals/ext-boost/libs" -Directory | ForEach-Object { Join-Path $_.FullName 'include' } | Where-Object {Test-Path -LiteralPath $_} | ForEach-Object {"/I$_"})
    $taskManifest=Get-Content "$PSScriptRoot/shader_dependency_inventory.json" -Raw | ConvertFrom-Json
    $taskResults=@()
    $taskBatchExit=0
    $taskSelected=$taskManifest.compile_sources
    $taskShimFlags=@()
    if($Settings){
        $taskSelected=@($taskSelected | Where-Object {[IO.File]::ReadAllText("$taskUp/$_").Contains('"core/emulator_settings.h"')})
        if($taskSelected.Count -ne 5){throw 'Settings inclusion census changed'}
        $taskShimFlags=@("/I$PSScriptRoot/shim")
    }
    if($Dump){$taskSelected=@((Get-Content "$PSScriptRoot/adapted/manifest.json" -Raw | ConvertFrom-Json) | ForEach-Object {$_.source});$taskShimFlags=@("/I$PSScriptRoot/shim","/I$PSScriptRoot")}
    if($Bounded){$taskBoundedManifest=Get-Content "$PSScriptRoot/adapted/bounded_manifest.json" -Raw | ConvertFrom-Json;$taskSelected=@($taskBoundedManifest.units | ForEach-Object {$_.source});$taskShimFlags=@("/I$PSScriptRoot/adapted/include","/I$PSScriptRoot/shim","/I$PSScriptRoot")}
    foreach($taskSource in $taskSelected){
        $taskName=$taskSource.Replace('/','_').Replace('.cpp','')
        $taskLog="$taskBuild/$taskName.log"
        $taskObject="$taskBuild/$taskName.obj"
        if($Settings){$taskObject="$taskBuild/$taskName.settings.obj";$taskLog="$taskBuild/$taskName.settings.log"}
        $taskCompileSource="$taskUp/$taskSource"
        if($Dump){$taskCompileSource="$PSScriptRoot/adapted/"+$taskSource.Replace('/','_');$taskObject="$taskBuild/$taskName.dump.obj";$taskLog="$taskBuild/$taskName.dump.log"}
        if($Bounded){$taskUnit=$taskBoundedManifest.units | Where-Object {$_.source -eq $taskSource};$taskCompileSource="$taskRoot/$($taskUnit.compile_source)";$taskObject="$taskBuild/$taskName.bounded.obj";$taskLog="$taskBuild/$taskName.bounded.log"}
        & $taskCompiler @taskShimFlags /utf-8 /nologo /c /O2 /W4 /EHsc /std:c++latest /DNOMINMAX @taskBoostIncludes "/I$taskUp/src" "/I$taskUp" "/I$taskUp/externals/vulkan-headers/include" "/I$taskUp/externals/xbyak" "/I$taskUp/externals/zydis/include" "/I$taskUp/externals/zydis/dependencies/zycore/include" "/I$taskUp/externals/ext-boost" "/I$taskUp/externals/spdlog/include" "/I$taskUp/externals/magic_enum/include" "/I$taskUp/externals/fmt/include" "/I$taskUp/externals/sirit/include" "/I$taskUp/externals/sirit/externals/SPIRV-Headers/include" "/I$taskUp/externals/json/include" "$taskCompileSource" "/Fo$taskObject" *> $taskLog
        $taskCode=$LASTEXITCODE
        $taskResults+=@{source=$taskSource;exit_code=$taskCode;log=$taskLog;object=$taskObject}
        if($taskCode -ne 0){
            $taskBatchExit=1
            if(-not $ContinueOnError){Get-Content $taskLog -Tail 16;break}
        }
    }
    $taskResultPath="$taskBuild/shader_compile_results.json"
    if($Settings){$taskResultPath="$taskBuild/shader_settings_results.json"}
    if($Dump){$taskResultPath="$taskBuild/shader_dump_results.json"}
    if($Bounded){$taskResultPath="$taskBuild/shader_bounded_results.json"}
    $taskResults | ConvertTo-Json -Depth 4 | Set-Content $taskResultPath
    Write-Output "Shader compile attempted=$($taskResults.Count) passed=$(@($taskResults | Where-Object {$_.exit_code -eq 0}).Count); link=false retail_shaders_tested=0"
    exit $taskBatchExit
}
& $taskCompiler /utf-8 /nologo /c /O2 /W4 /EHsc /std:c++latest /DNOMINMAX "/I$taskUp/src" "/I$taskUp" "/I$taskUp/externals/vulkan-headers/include" "/I$taskUp/externals/xbyak" "/I$taskUp/externals/zydis/include" "/I$taskUp/externals/zydis/dependencies/zycore/include" "/I$taskUp/externals/ext-boost" "/I$taskUp/externals/spdlog/include" "/I$taskUp/externals/magic_enum/include" "/I$taskUp/externals/fmt/include" "$taskUp/src/shader_recompiler/frontend/decode.cpp" "/Fo$taskBuild/shader_decode.obj" *> "$taskBuild/shader_build.log"
$taskExit=$LASTEXITCODE
$ErrorActionPreference='Stop'
Get-Content "$taskBuild/shader_build.log" -Tail 12
exit $taskExit
